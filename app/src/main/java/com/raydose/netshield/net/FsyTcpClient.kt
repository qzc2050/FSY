package com.raydose.netshield.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.os.Build
import android.util.Log
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.concurrent.thread

/**
 * TCP 客户端：连接从机控制口。
 *
 * 工控 ROM 上会对同一目标尝试多条 [HostRoute] 与多种 bind 组合（先 Network.bindSocket，再仅 bind 本机 IP，最后直连）。
 */
class FsyTcpClient(
    private val appContext: Context,
    private val onReceive: (ByteArray) -> Unit,
    private val onError: (String) -> Unit,
    private val onRemoteDisconnected: () -> Unit = {},
) {
    @Volatile
    private var running = false

    private var socket: Socket? = null

    fun connect(
        host: String,
        port: Int,
        timeoutMs: Int = 8000,
        preferredNetwork: Network? = null,
        preferredLocalIpv4: String? = null,
    ): Boolean {
        close()
        val routes = buildRouteAttempts(host, preferredNetwork, preferredLocalIpv4)
        var lastError = "无可用路由"
        for (attempt in routes) {
            val result = tryConnectOnce(host, port, timeoutMs, attempt)
            if (result.success) {
                Log.i(TAG, "TCP 已连接 $host:$port via ${attempt.label}")
                return true
            }
            lastError = result.error ?: lastError
            Log.d(TAG, "TCP 尝试失败 [$host:$port] ${attempt.label}: $lastError")
            if (isConnectionRefused(lastError)) {
                Log.d(TAG, "TCP $host:$port ECONNREFUSED，跳过其余路由（从机 TCP 尚未就绪）")
                break
            }
        }
        onError("$lastError — 已尝试 ${routes.size} 种路由/绑定组合")
        return false
    }

    private fun buildRouteAttempts(
        host: String,
        preferredNetwork: Network?,
        preferredLocalIpv4: String?,
    ): List<ConnectAttempt> {
        val attempts = linkedSetOf<ConnectAttempt>()
        findRoutesToHost(appContext, host).forEach { route ->
            attempts += ConnectAttempt(
                network = route.network,
                localIpv4 = route.localIpv4,
                label = "route ${route.interfaceName}/${route.localIpv4} (${route.source})",
            )
        }
        val local = preferredLocalIpv4?.trim()?.takeIf { it.isNotEmpty() }
        if (local != null) {
            attempts += ConnectAttempt(
                network = preferredNetwork,
                localIpv4 = local,
                label = "preferred $local net=$preferredNetwork",
            )
        }
        if (preferredNetwork != null && local == null) {
            attempts += ConnectAttempt(preferredNetwork, null, "preferred net only")
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val cm = appContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            cm.activeNetwork?.let { net ->
                attempts += ConnectAttempt(net, null, "activeNetwork")
            }
        }
        attempts += ConnectAttempt(null, null, "plain")
        return attempts.toList()
    }

    private data class ConnectAttempt(
        val network: Network?,
        val localIpv4: String?,
        val label: String,
    )

    private data class ConnectResult(val success: Boolean, val error: String?)

    private enum class BindStrategy(val description: String) {
        NetworkOnly("bindSocket"),
        LocalOnly("bind local"),
        NetworkThenLocal("bindSocket+local"),
        Plain("plain"),
    }

    private fun tryConnectOnce(
        host: String,
        port: Int,
        timeoutMs: Int,
        attempt: ConnectAttempt,
    ): ConnectResult {
        val strategies = listOf(
            BindStrategy.NetworkOnly,
            BindStrategy.LocalOnly,
            BindStrategy.NetworkThenLocal,
            BindStrategy.Plain,
        )
        var last: String? = null
        for (strategy in strategies) {
            val r = tryConnectWithStrategy(host, port, timeoutMs, attempt, strategy)
            if (r.success) return r
            last = r.error
            if (isConnectionRefused(last)) break
        }
        return ConnectResult(false, last)
    }

    private fun tryConnectWithStrategy(
        host: String,
        port: Int,
        timeoutMs: Int,
        attempt: ConnectAttempt,
        strategy: BindStrategy,
    ): ConnectResult {
        val s = Socket()
        try {
            s.tcpNoDelay = true
            val apiM = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
            val local = attempt.localIpv4?.trim()?.takeIf { it.isNotEmpty() }
            val network = attempt.network

            when (strategy) {
                BindStrategy.NetworkOnly -> {
                    if (apiM && network != null) {
                        network.bindSocket(s)
                    } else {
                        return ConnectResult(false, "skip: no network")
                    }
                }
                BindStrategy.LocalOnly -> {
                    if (local != null) {
                        s.bind(InetSocketAddress(InetAddress.getByName(local), 0))
                    } else {
                        return ConnectResult(false, "skip: no local")
                    }
                }
                BindStrategy.NetworkThenLocal -> {
                    if (apiM && network != null) {
                        try {
                            network.bindSocket(s)
                        } catch (_: Exception) {
                        }
                    }
                    if (local != null) {
                        try {
                            s.bind(InetSocketAddress(InetAddress.getByName(local), 0))
                        } catch (_: Exception) {
                        }
                    }
                }
                BindStrategy.Plain -> Unit
            }

            s.connect(InetSocketAddress(host, port), timeoutMs)
            socket = s
            return ConnectResult(true, null)
        } catch (e: Exception) {
            try {
                s.close()
            } catch (_: Exception) {
            }
            val detail = buildString {
                append(strategy.description)
                append(" | ").append(attempt.label)
                append(" | ").append(e.message ?: e.javaClass.simpleName)
            }
            return ConnectResult(false, detail)
        }
    }

    /** 在连接成功且调用方确认仍有效后再启动读线程，避免被 supersede 的连接启动 reader 后崩溃。 */
    fun beginReading() {
        val s = socket ?: return
        if (running) return
        running = true
        startReader(s)
    }

    private fun startReader(s: Socket) {
        thread(name = "fsy-tcp-read", isDaemon = true) {
            try {
                if (!running) return@thread
                val input = s.getInputStream()
                val buf = ByteArray(4096)
                while (running) {
                    try {
                        val n = input.read(buf)
                        if (n < 0) {
                            if (running) {
                                clearSocketLocked()
                                onRemoteDisconnected()
                            }
                            break
                        }
                        if (n > 0) {
                            onReceive(buf.copyOf(n))
                        }
                    } catch (e: Exception) {
                        if (running) {
                            clearSocketLocked()
                            onError(e.message ?: "TCP 读失败")
                        }
                        break
                    }
                }
            } catch (e: Exception) {
                if (running) {
                    clearSocketLocked()
                    onError(e.message ?: "TCP 读失败")
                }
            }
        }
    }

    fun send(data: ByteArray) {
        try {
            socket?.getOutputStream()?.apply {
                write(data)
                flush()
            }
        } catch (e: Exception) {
            if (running) {
                clearSocketLocked()
                onError(e.message ?: "TCP 发送失败")
            }
        }
    }

    fun close() {
        running = false
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        socket = null
    }

    private fun clearSocketLocked() {
        running = false
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        socket = null
    }

    companion object {
        private const val TAG = "NetShield"

        private fun isConnectionRefused(message: String?): Boolean {
            if (message.isNullOrBlank()) return false
            return message.contains("ECONNREFUSED", ignoreCase = true) ||
                message.contains("Connection refused", ignoreCase = true)
        }
    }
}
