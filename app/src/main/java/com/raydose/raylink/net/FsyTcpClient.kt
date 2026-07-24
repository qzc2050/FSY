package com.raydose.raylink.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.os.Build
import android.util.Log
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.concurrent.thread

/**
 * TCP 客户端：连接从机控制口。
 *
 * 工控 ROM 上会对同一目标尝试多条 [HostRoute] 与多种 bind 组合（先 Network.bindSocket，再仅 bind 本机 IP，最后直连）。
 * 收发共用 [ioLock]，避免读线程关 socket 与写线程并发导致假成功/Broken pipe。
 */
class FsyTcpClient(
    private val appContext: Context,
    private val onReceive: (ByteArray) -> Unit,
    private val onError: (String) -> Unit,
    private val onRemoteDisconnected: () -> Unit = {},
) {
    private val ioLock = Any()

    @Volatile
    private var running = false

    @Volatile
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
        // 连接失败由调用方处理；勿走 onError，否则会与「会话丢失→立即重连」形成死循环
        Log.w(TAG, "TCP 连接失败 $host:$port: $lastError — 已尝试 ${routes.size} 种路由/绑定组合")
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
            s.keepAlive = true
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
            synchronized(ioLock) {
                socket = s
            }
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
        val s = synchronized(ioLock) {
            if (running) return
            val sock = socket ?: return
            running = true
            sock
        }
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
                            var notify = false
                            synchronized(ioLock) {
                                if (running && socket === s) {
                                    clearSocketLocked()
                                    notify = true
                                }
                            }
                            if (notify) onRemoteDisconnected()
                            break
                        }
                        if (n > 0) {
                            onReceive(buf.copyOf(n))
                        }
                    } catch (e: Exception) {
                        var notify = false
                        var msg = ""
                        synchronized(ioLock) {
                            if (running && socket === s) {
                                clearSocketLocked()
                                notify = true
                                msg = "${e.javaClass.simpleName}: ${e.message ?: "TCP 读失败"}"
                            }
                        }
                        if (notify) {
                            Log.w(TAG, "TCP 读异常: $msg", e)
                            onError(msg)
                        }
                        break
                    }
                }
            } catch (e: Exception) {
                var notify = false
                var msg = ""
                synchronized(ioLock) {
                    if (running && socket === s) {
                        clearSocketLocked()
                        notify = true
                        msg = "${e.javaClass.simpleName}: ${e.message ?: "TCP 读失败"}"
                    }
                }
                if (notify) {
                    Log.w(TAG, "TCP 读异常: $msg", e)
                    onError(msg)
                }
            }
        }
    }

    /**
     * @return true 表示字节已写出；false 表示未发送（无 socket / 已关闭 / IO 异常）。
     * IO 异常时会关连接并回调 [onError]。
     */
    fun send(data: ByteArray): Boolean {
        var errorMsg: String? = null
        var errorEx: Exception? = null
        val ok = synchronized(ioLock) {
            val s = socket
            if (s == null || !running) {
                Log.w(
                    TAG,
                    "TCP 发送跳过: socketNull=${s == null} running=$running len=${data.size} " +
                        "thread=${Thread.currentThread().name}",
                )
                return@synchronized false
            }
            try {
                val out = s.getOutputStream()
                out.write(data)
                out.flush()
                true
            } catch (e: Exception) {
                errorEx = e
                errorMsg = "${e.javaClass.simpleName}: ${e.message ?: "TCP 发送失败"}"
                // 主线程误发不算链路坏了，勿拆连接；真正 socket IO 错误才关
                if (running && e !is android.os.NetworkOnMainThreadException) {
                    clearSocketLocked()
                }
                false
            }
        }
        if (!ok && errorMsg != null) {
            Log.w(TAG, "TCP 发送失败: $errorMsg", errorEx)
            // NetworkOnMainThreadException 不应触发重连风暴
            if (errorEx !is android.os.NetworkOnMainThreadException) {
                onError(errorMsg)
            }
        }
        return ok
    }

    fun close() {
        synchronized(ioLock) {
            running = false
            try {
                socket?.close()
            } catch (_: Exception) {
            }
            socket = null
        }
    }

    /** 调用方须已持有 [ioLock] */
    private fun clearSocketLocked() {
        running = false
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        socket = null
    }

    companion object {
        private const val TAG = "Raylink"

        private fun isConnectionRefused(message: String?): Boolean {
            if (message.isNullOrBlank()) return false
            return message.contains("ECONNREFUSED", ignoreCase = true) ||
                message.contains("Connection refused", ignoreCase = true)
        }
    }
}
