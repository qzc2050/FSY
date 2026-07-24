package com.raydose.raylink.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.wifi.WifiManager
import android.os.Build
import java.net.DatagramPacket
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.MulticastSocket
import java.net.NetworkInterface
import java.util.Collections

/**
 * 监听 236.2.3.6:2468 组播发现（与协议文档一致）。
 *
 * 使用 [ConnectivityManager] 在可用时将 [MulticastSocket] 绑定到所选 [Network]，
 * 并与 [preferredInterfaceName] 指定的网卡做 join，避免 Wi‑Fi+eth 并存时走错线路。
 */
class FsyMulticastDiscovery(
    private val appContext: Context,
    private val onDatagram: (String) -> Unit,
    private val onError: (String) -> Unit,
    private val onInfo: (String) -> Unit = {},
) {
    companion object {
        private const val GROUP = "236.2.3.6"
        private const val PORT = 2468
    }

    @Volatile
    private var running = false

    private var socket: MulticastSocket? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    private var thread: Thread? = null
    private var boundNetwork: Network? = null
    private var boundInterfaceName: String? = null

    fun start(preferredNetwork: Network? = null, preferredInterfaceName: String? = null) {
        // 若上次未完全释放（快速切换、异常等），先 stop，避免 if (running) return 导致“看似已开始、实际未监听”
        if (running) {
            stop()
        }
        running = true
        boundNetwork = preferredNetwork
        boundInterfaceName = preferredInterfaceName
        val wifi = appContext.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wifi.createMulticastLock("raylink.mcast").apply {
            setReferenceCounted(false)
            acquire()
        }
        thread = Thread({
            var ms: MulticastSocket? = null
            try {
                ms = MulticastSocket(PORT).apply { reuseAddress = true }
                socket = ms

                val cm =
                    appContext.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
                // 仅在有明确 Network 句柄时 bindSocket；若只有网卡名（iface: 条目 network=null），
                // 不要用 activeNetwork 误绑到 Wi‑Fi，否则与 join 的 eth1/wlan 不一致。
                val targetNetwork: Network? = when {
                    preferredNetwork != null -> preferredNetwork
                    preferredInterfaceName.isNullOrBlank() -> cm.activeNetwork
                    else -> null
                }
                if (targetNetwork != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    try {
                        targetNetwork.bindSocket(ms)
                        onInfo("已将组播套接字绑定到所选 Network")
                    } catch (e: Exception) {
                        onInfo("bindSocket 跳过: ${e.message}")
                    }
                } else if (targetNetwork == null && preferredInterfaceName.isNullOrBlank()) {
                    onInfo("未指定网卡且无 activeNetwork，仍尝试监听")
                } else if (targetNetwork == null) {
                    onInfo("按网卡名 join 组播（无 Network bindSocket）: $preferredInterfaceName")
                }

                val groupAddr = InetAddress.getByName(GROUP)
                val netIf = preferredInterfaceName?.let { safeGetNetworkInterface(it) }
                    ?: resolveMulticastNetworkInterface(cm, targetNetwork)
                    ?: findWifiOrFirstNonLoopbackInterface()

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && netIf != null) {
                    try {
                        ms.joinGroup(InetSocketAddress(groupAddr, PORT), netIf)
                        onInfo("已 join 组播: $GROUP:$PORT, 网卡=${netIf.name}")
                    } catch (e: Exception) {
                        onInfo("joinGroup(指定网卡) 失败，尝试旧 API: ${e.message}")
                        @Suppress("DEPRECATION")
                        ms.joinGroup(groupAddr)
                        onInfo("已 join 组播(旧 API): $GROUP")
                    }
                } else {
                    @Suppress("DEPRECATION")
                    ms.joinGroup(groupAddr)
                    onInfo("已 join 组播(旧 API): $GROUP, 网卡=${netIf?.name ?: "null"}")
                }

                val buf = ByteArray(2048)
                while (running) {
                    val pkt = DatagramPacket(buf, buf.size)
                    ms.receive(pkt)
                    val len = pkt.length
                    if (len <= 0) continue
                    val text = String(pkt.data, 0, len, Charsets.UTF_8).trim()
                    if (text.isNotEmpty()) {
                        onDatagram(text)
                    }
                }
            } catch (e: Exception) {
                if (running) {
                    onError(e.message ?: "组播接收异常: ${e.javaClass.simpleName}")
                }
            } finally {
                try {
                    val groupAddr = InetAddress.getByName(GROUP)
                    val cm =
                        appContext.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
                    val netIf = boundInterfaceName?.let { safeGetNetworkInterface(it) }
                        ?: resolveMulticastNetworkInterface(cm, boundNetwork ?: cm.activeNetwork)
                        ?: findWifiOrFirstNonLoopbackInterface()
                    if (ms != null) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && netIf != null) {
                            try {
                                ms.leaveGroup(InetSocketAddress(groupAddr, PORT), netIf)
                            } catch (_: Exception) {
                                @Suppress("DEPRECATION")
                                ms.leaveGroup(groupAddr)
                            }
                        } else {
                            @Suppress("DEPRECATION")
                            ms.leaveGroup(groupAddr)
                        }
                    }
                } catch (_: Exception) {
                }
                try {
                    ms?.close()
                } catch (_: Exception) {
                }
                socket = null
                running = false
            }
        }, "fsy-mcast").also { it.start() }
    }

    /**
     * 用当前活跃网络的 IPv4 地址反查 [NetworkInterface]，比按 wlan 名字猜更可靠。
     */
    private fun resolveMulticastNetworkInterface(
        cm: ConnectivityManager,
        network: Network?,
    ): NetworkInterface? {
        if (network == null) return null
        return try {
            val lp = cm.getLinkProperties(network) ?: return null
            for (la in lp.linkAddresses) {
                val a = la.address
                if (a is Inet4Address && !a.isLoopbackAddress) {
                    return NetworkInterface.getByInetAddress(a)
                }
            }
            null
        } catch (_: Exception) {
            null
        }
    }

    private fun findWifiOrFirstNonLoopbackInterface(): NetworkInterface? {
        return try {
            val list = Collections.list(NetworkInterface.getNetworkInterfaces())
            list.firstOrNull { it.name.startsWith("wlan", ignoreCase = true) }
                ?: list.firstOrNull { iface ->
                    iface.inetAddresses.asSequence().any { !it.isLoopbackAddress && it is Inet4Address }
                }
        } catch (_: Exception) {
            null
        }
    }

    private fun safeGetNetworkInterface(name: String): NetworkInterface? {
        return try {
            NetworkInterface.getByName(name)
        } catch (_: Exception) {
            null
        }
    }

    fun stop() {
        running = false
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        socket = null
        boundNetwork = null
        boundInterfaceName = null
        try {
            thread?.join(3000)
        } catch (_: Exception) {
        }
        thread = null
        try {
            multicastLock?.release()
        } catch (_: Exception) {
        }
        multicastLock = null
    }
}
