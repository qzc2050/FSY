package com.raydose.raylink.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkAddress
import android.net.Network
import android.os.Build
import java.net.Inet4Address
import java.net.InetAddress
import java.net.NetworkInterface
import java.util.Collections

/**
 * 为访问指定 IPv4 主机选择出站路由（工控机多网口 / 无默认网络时常用）。
 */
data class HostRoute(
    val network: Network?,
    val localIpv4: String,
    val interfaceName: String,
    val fromConnectivity: Boolean,
    val source: String,
)

fun findRoutesToHost(appContext: Context, host: String): List<HostRoute> {
    val target = runCatching { InetAddress.getByName(host.trim()) }.getOrNull() as? Inet4Address
        ?: return emptyList()

    val cm = appContext.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    val routes = linkedSetOf<HostRoute>()
    val seenLocal = mutableSetOf<String>()

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        @Suppress("DEPRECATION")
        for (network in cm.allNetworks) {
            val lp = cm.getLinkProperties(network) ?: continue
            val iface = lp.interfaceName ?: "unknown"
            for (la in lp.linkAddresses) {
                val local = la.address as? Inet4Address ?: continue
                if (local.isLoopbackAddress) continue
                val localIp = local.hostAddress ?: continue
                if (!isReachableVia(local, target, la.prefixLength)) continue
                if (seenLocal.add(localIp)) {
                    routes += HostRoute(
                        network = network,
                        localIpv4 = localIp,
                        interfaceName = iface,
                        fromConnectivity = true,
                        source = "ConnectivityManager",
                    )
                }
            }
        }
    }

    Collections.list(NetworkInterface.getNetworkInterfaces()).forEach { ni ->
        if (!ni.isUp || ni.isLoopback) return@forEach
        val iface = ni.name ?: return@forEach
        ni.inetAddresses.asSequence()
            .filterIsInstance<Inet4Address>()
            .filter { !it.isLoopbackAddress }
            .forEach { local ->
                val localIp = local.hostAddress ?: return@forEach
                if (!isReachableVia(local, target, prefixLengthForInterface(ni, local))) return@forEach
                if (seenLocal.add(localIp)) {
                    routes += HostRoute(
                        network = findNetworkWithLocalIp(cm, localIp),
                        localIpv4 = localIp,
                        interfaceName = iface,
                        fromConnectivity = false,
                        source = "NetworkInterface",
                    )
                }
            }
    }

    return routes.sortedWith(
        compareBy<HostRoute> { routePriority(it.interfaceName) }
            .thenByDescending { it.fromConnectivity }
            .thenBy { it.localIpv4 },
    )
}

/** 与 [listFsyNetworkOptions] 默认策略一致：有线优先于 Wi‑Fi */
fun pickBestRoute(appContext: Context, host: String): HostRoute? =
    findRoutesToHost(appContext, host).firstOrNull()

private fun routePriority(iface: String): Int {
    val n = iface.lowercase()
    return when {
        n.startsWith("eth") -> 0
        n.startsWith("wlan") -> 1
        n.startsWith("usb") -> 2
        else -> 9
    }
}

private fun isReachableVia(local: Inet4Address, target: Inet4Address, prefixLength: Int): Boolean {
    val prefix = prefixLength.coerceIn(0, 32)
    if (prefix == 0) {
        // 无掩码时：私网同 /24 粗匹配（常见现场配置）
        return local.hostAddress?.substringBeforeLast('.') == target.hostAddress?.substringBeforeLast('.')
    }
    val mask = if (prefix == 32) -1 else (-1 shl (32 - prefix))
    return (ipv4ToInt(local) and mask) == (ipv4ToInt(target) and mask)
}

private fun ipv4ToInt(addr: Inet4Address): Int {
    val b = addr.address
    return ((b[0].toInt() and 0xff) shl 24) or
        ((b[1].toInt() and 0xff) shl 16) or
        ((b[2].toInt() and 0xff) shl 8) or
        (b[3].toInt() and 0xff)
}

private fun prefixLengthForInterface(ni: NetworkInterface, local: Inet4Address): Int {
  ni.interfaceAddresses
        .firstOrNull { it.address == local }
        ?.networkPrefixLength
        ?.toInt()
        ?.let { return it }
    return 24
}

private fun findNetworkWithLocalIp(cm: ConnectivityManager, ipv4: String): Network? {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return null
    @Suppress("DEPRECATION")
    for (network in cm.allNetworks) {
        val lp = cm.getLinkProperties(network) ?: continue
        for (la in lp.linkAddresses) {
            val a = la.address
            if (a is Inet4Address && !a.isLoopbackAddress && ipv4 == a.hostAddress) {
                return network
            }
        }
    }
    return null
}
