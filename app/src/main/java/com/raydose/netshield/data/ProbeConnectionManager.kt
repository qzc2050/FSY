package com.raydose.netshield.data

import android.content.Context
import android.net.Network
import android.util.Log
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.net.FsyBroadcast
import com.raydose.netshield.net.FsyProtocolFrameCollector
import com.raydose.netshield.net.FsyMulticastDiscovery
import com.raydose.netshield.model.modbusDeviceAddr
import com.raydose.netshield.net.FsyTcpClient
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.findRoutesToHost
import com.raydose.netshield.net.listFsyNetworkOptions
import com.raydose.netshield.net.parseFsyBroadcast
import com.raydose.netshield.net.parseFsyTcpFrame
import com.raydose.netshield.net.pickBestRoute
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

/**
 * 组播发现 + 对已保存探头维持 TCP，解析 0x23 实时数据。
 * 断电/断线后：退避自动重连 + 组播再次出现立即重连。
 */
class ProbeConnectionManager(
    context: Context,
    private val onDiscoveredRaw: (String) -> Unit,
    private val onTcpFrame: (probeId: String, frame: com.raydose.netshield.net.ParsedFsyFrame) -> Unit,
    private val onProbeOnlineChanged: (probeId: String, online: Boolean) -> Unit,
    private val onLog: (String) -> Unit = {},
) {
    private val appContext = context.applicationContext
    private val collectors = ConcurrentHashMap<String, FsyProtocolFrameCollector>()
    private val clients = ConcurrentHashMap<String, FsyTcpClient>()
    private val connectGeneration = ConcurrentHashMap<String, AtomicInteger>()
    private val probeOnline = ConcurrentHashMap<String, Boolean>()
    private val savedProbesRef = AtomicReference<List<SavedProbe>>(emptyList())
    private val retryDelayMs = ConcurrentHashMap<String, AtomicLong>()
    private val reconnectGate = ConcurrentHashMap<String, AtomicBoolean>()
    private val lastDiscoveryReconnectMs = ConcurrentHashMap<String, Long>()
    private val lastConnectAttemptMs = ConcurrentHashMap<String, Long>()
    private val connectLocks = ConcurrentHashMap<String, Any>()

    @Volatile
    private var preferredNetwork: Network? = null

    @Volatile
    private var preferredInterfaceName: String? = null

    @Volatile
    private var preferredLocalIpv4: String? = null

    @Volatile
    private var suppressAutoReconnect = false

    @Volatile
    private var watchdogRunning = false

    private var watchdogThread: Thread? = null

    private val discovery = FsyMulticastDiscovery(
        appContext = appContext,
        onDatagram = { text ->
            onDiscoveredRaw(text)
            parseFsyBroadcast(text)?.let { broadcast ->
                onLog("发现 ${broadcast.model} ${broadcast.ip} id=${broadcast.protoAddr}")
                tryReconnectOnDiscovery(broadcast)
            }
        },
        onError = { msg -> Log.w(TAG, "组播: $msg") },
        onInfo = { msg -> Log.d(TAG, "组播: $msg") },
    )

    fun setSavedProbes(probes: List<SavedProbe>) {
        savedProbesRef.set(probes.toList())
    }

    fun refreshNetworkPreference() {
        val options = listFsyNetworkOptions(appContext)
        val pick = options.firstOrNull { it.isDefault }
            ?: options.firstOrNull()
        preferredNetwork = pick?.network
        preferredInterfaceName = pick?.interfaceName
        preferredLocalIpv4 = pick?.localIpv4
    }

    fun startDiscovery() {
        refreshNetworkPreference()
        discovery.start(preferredNetwork, preferredInterfaceName)
        startWatchdog()
        Log.i(TAG, "组播发现已启动 iface=$preferredInterfaceName")
    }

    fun stopDiscovery() {
        watchdogRunning = false
        watchdogThread?.interrupt()
        watchdogThread = null
        discovery.stop()
    }

    fun reconnectAll(probes: List<SavedProbe>) {
        setSavedProbes(probes)
        suppressAutoReconnect = true
        try {
            disconnectAll()
            refreshNetworkPreference()
            probes.forEach { probe ->
                resetRetryDelay(probe.id)
                scheduleConnect(probe)
            }
        } finally {
            suppressAutoReconnect = false
        }
    }

    fun connect(probe: SavedProbe) {
        resetRetryDelay(probe.id)
        scheduleConnect(probe)
    }

    /**
     * 进入探头管理时按需读取：辐射上下阈值(0x40×2)、报警使能(0x52)、音量(0x7A)、controlbit2(0x7B)。
     * 应答经 [onTcpFrame] 回调，由 ViewModel 合并到草稿。
     */
    fun fetchManageConfig(probe: SavedProbe) {
        if (!isProbeOnline(probe.id)) return
        val client = clients[probe.id] ?: return
        val addr = probe.modbusDeviceAddr()
        val reads = listOf(
            buildReadRegsFrame(0x0040, 4, addr),
            buildReadRegsFrame(0x0052, 2, addr),
            buildReadRegsFrame(0x007A, 1, addr),
            buildReadRegsFrame(0x007B, 2, addr),
        )
        thread(name = "fsy-fetch-cfg-${probe.id}") {
            reads.forEach { frame ->
                if (!isProbeOnline(probe.id)) return@thread
                client.send(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
            onLog("已请求 ${probe.displayName} 探头管理配置")
        }
    }

    fun sendFrames(probeId: String, frames: List<ByteArray>) {
        if (!isProbeOnline(probeId)) return
        val client = clients[probeId] ?: return
        thread(name = "fsy-write-cfg-$probeId") {
            frames.forEach { frame ->
                if (!isProbeOnline(probeId)) return@thread
                client.send(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
        }
    }

    private fun scheduleConnect(probe: SavedProbe) {
        lastConnectAttemptMs[probe.id] = System.currentTimeMillis()
        val generation = nextConnectGeneration(probe.id)
        thread(name = "fsy-tcp-connect-${probe.id}") {
            val lock = connectLocks.getOrPut(probe.id) { Any() }
            synchronized(lock) {
                if (isStaleConnect(probe.id, generation)) return@synchronized
                connectOnWorker(probe, generation)
            }
        }
    }

    private fun nextConnectGeneration(probeId: String): Int =
        connectGeneration.getOrPut(probeId) { AtomicInteger(0) }.incrementAndGet()

    private fun isStaleConnect(probeId: String, generation: Int): Boolean {
        val current = connectGeneration[probeId]?.get() ?: return true
        return generation != current
    }

    private fun isProbeOnline(probeId: String): Boolean = probeOnline[probeId] == true

    private fun setProbeOnline(probeId: String, online: Boolean) {
        val was = probeOnline[probeId]
        probeOnline[probeId] = online
        if (was != online) {
            onProbeOnlineChanged(probeId, online)
        }
        if (online) {
            resetRetryDelay(probeId)
        }
    }

    private fun handleConnectionLost(probe: SavedProbe, reason: String) {
        if (!isProbeOnline(probe.id) && clients[probe.id] == null) return
        Log.i(TAG, "TCP 断开 ${probe.displayName}@${probe.ip}: $reason")
        closeClientOnly(probe.id)
        setProbeOnline(probe.id, false)
        if (reason.contains("对端关闭") || reason.contains("ECONNREFUSED")) {
            applyBootingRetryDelay(probe.id)
        }
        scheduleReconnect(probe.id)
    }

    private fun connectOnWorker(probe: SavedProbe, generation: Int) {
        if (isStaleConnect(probe.id, generation)) return
        closeClientOnly(probe.id)
        if (isStaleConnect(probe.id, generation)) return

        Log.i(TAG, "TCP 开始连接 ${probe.displayName} ${probe.ip}:${probe.controlPort} (gen=$generation)")

        val collector = FsyProtocolFrameCollector()
        collectors[probe.id] = collector
        val client = FsyTcpClient(
            appContext = appContext,
            onReceive = { chunk ->
                val frames = collector.feed(chunk)
                frames.forEach { frame ->
                    parseFsyTcpFrame(frame)?.let { parsed ->
                        onTcpFrame(probe.id, parsed)
                    }
                }
            },
            onError = { msg ->
                Log.w(TAG, "TCP ${probe.displayName}@${probe.ip}: $msg")
                if (isProbeOnline(probe.id)) {
                    handleConnectionLost(probe, msg)
                }
            },
            onRemoteDisconnected = {
                handleConnectionLost(probe, "对端关闭连接")
            },
        )
        clients[probe.id] = client

        val route = pickBestRoute(appContext, probe.ip)
        if (route != null) {
            Log.i(
                TAG,
                "TCP 路由 ${probe.ip} -> ${route.interfaceName}/${route.localIpv4} net=${route.network} (${route.source})",
            )
        } else {
            val all = findRoutesToHost(appContext, probe.ip)
            Log.w(TAG, "TCP 未匹配到与 ${probe.ip} 同网段路由，候选=${all.size} 将回退默认网卡")
        }

        if (isStaleConnect(probe.id, generation)) {
            client.close()
            closeClientOnly(probe.id)
            return
        }

        val ok = client.connect(
            host = probe.ip,
            port = probe.controlPort,
            preferredNetwork = route?.network ?: preferredNetwork,
            preferredLocalIpv4 = route?.localIpv4 ?: preferredLocalIpv4,
        )

        if (isStaleConnect(probe.id, generation)) {
            client.close()
            closeClientOnly(probe.id)
            return
        }

        if (ok) {
            setProbeOnline(probe.id, true)
            Log.i(TAG, "TCP 已连接 ${probe.displayName} ${probe.ip}:${probe.controlPort}")
        } else {
            setProbeOnline(probe.id, false)
            applyBootingRetryDelay(probe.id)
            Log.w(TAG, "TCP 连接失败 ${probe.ip}:${probe.controlPort}，将自动重试")
            scheduleReconnect(probe.id)
        }
    }

    /** 从机上电后 TCP 常晚于组播就绪，避免 3s 就刷 ECONNREFUSED */
    private fun applyBootingRetryDelay(probeId: String) {
        retryDelayMs.getOrPut(probeId) { AtomicLong(INITIAL_RETRY_MS) }
            .updateAndGet { current -> maxOf(current, BOOTING_RETRY_MS) }
    }

    /** 组播再次发现已保存探头且当前离线 → 尽快重连（上电恢复） */
    private fun tryReconnectOnDiscovery(broadcast: FsyBroadcast) {
        if (suppressAutoReconnect) return
        val device = DiscoveredDevice.fromBroadcast(broadcast)
        val probe = savedProbesRef.get().find { saved ->
            saved.id == device.stableId ||
                (saved.ip == device.ip && saved.protoAddr == device.protoAddr)
        } ?: return
        if (isProbeOnline(probe.id)) return

        val now = System.currentTimeMillis()
        val lastDiscovery = lastDiscoveryReconnectMs[probe.id] ?: 0L
        if (now - lastDiscovery < DISCOVERY_RECONNECT_DEBOUNCE_MS) return
        val lastAttempt = lastConnectAttemptMs[probe.id] ?: 0L
        if (now - lastAttempt < CONNECT_COOLDOWN_MS) return

        lastDiscoveryReconnectMs[probe.id] = now
        resetRetryDelay(probe.id)
        Log.i(TAG, "组播发现离线探头 ${probe.displayName}，触发重连")
        scheduleConnect(probe)
    }

    private fun scheduleReconnect(probeId: String) {
        if (suppressAutoReconnect) return
        if (savedProbesRef.get().none { it.id == probeId }) return
        if (isProbeOnline(probeId)) return

        val gate = reconnectGate.getOrPut(probeId) { AtomicBoolean(false) }
        if (!gate.compareAndSet(false, true)) return

        val delay = currentRetryDelay(probeId)
        thread(name = "fsy-tcp-retry-$probeId") {
            try {
                Thread.sleep(delay)
                bumpRetryDelay(probeId)
                if (suppressAutoReconnect || isProbeOnline(probeId)) return@thread
                val probe = savedProbesRef.get().find { it.id == probeId } ?: return@thread
                Log.i(TAG, "TCP 自动重连 ${probe.displayName}（${delay}ms 后）")
                scheduleConnect(probe)
            } catch (_: InterruptedException) {
            } finally {
                gate.set(false)
            }
        }
    }

    private fun currentRetryDelay(probeId: String): Long =
        retryDelayMs.getOrPut(probeId) { AtomicLong(INITIAL_RETRY_MS) }.get()

    private fun bumpRetryDelay(probeId: String) {
        val ref = retryDelayMs.getOrPut(probeId) { AtomicLong(INITIAL_RETRY_MS) }
        val next = (ref.get() * 1.5).toLong().coerceIn(INITIAL_RETRY_MS, MAX_RETRY_MS)
        ref.set(next)
    }

    private fun resetRetryDelay(probeId: String) {
        retryDelayMs.getOrPut(probeId) { AtomicLong(INITIAL_RETRY_MS) }.set(INITIAL_RETRY_MS)
    }

    private fun shouldWatchdogRetry(probeId: String): Boolean {
        val last = lastConnectAttemptMs[probeId] ?: 0L
        return System.currentTimeMillis() - last >= WATCHDOG_MIN_GAP_MS
    }

    private fun startWatchdog() {
        if (watchdogRunning) return
        watchdogRunning = true
        watchdogThread = thread(name = "fsy-tcp-watchdog", isDaemon = true) {
            while (watchdogRunning) {
                try {
                    Thread.sleep(WATCHDOG_INTERVAL_MS)
                    if (suppressAutoReconnect) continue
                    savedProbesRef.get().forEach { probe ->
                        if (!isProbeOnline(probe.id) && shouldWatchdogRetry(probe.id)) {
                            scheduleReconnect(probe.id)
                        }
                    }
                } catch (_: InterruptedException) {
                    break
                }
            }
        }
    }

    fun disconnect(probeId: String) {
        nextConnectGeneration(probeId)
        closeClientOnly(probeId)
        setProbeOnline(probeId, false)
    }

    fun disconnectAll() {
        clients.keys.toList().forEach { probeId ->
            nextConnectGeneration(probeId)
            closeClientOnly(probeId)
            setProbeOnline(probeId, false)
        }
    }

    private fun closeClientOnly(probeId: String) {
        clients.remove(probeId)?.close()
        collectors.remove(probeId)
    }

    companion object {
        const val TAG = "NetShield"
        private const val INITIAL_RETRY_MS = 3_000L
        private const val BOOTING_RETRY_MS = 10_000L
        private const val MAX_RETRY_MS = 30_000L
        private const val DISCOVERY_RECONNECT_DEBOUNCE_MS = 4_000L
        private const val CONNECT_COOLDOWN_MS = 5_000L
        private const val WATCHDOG_INTERVAL_MS = 20_000L
        private const val WATCHDOG_MIN_GAP_MS = 18_000L
        private const val CONFIG_READ_GAP_MS = 100L
    }
}
