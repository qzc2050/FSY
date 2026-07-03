package com.raydose.netshield.data

import android.content.Context
import android.net.Network
import android.util.Log
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.NeijiProbeRegs
import com.raydose.netshield.model.ProbeCommandLink
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.matchesSaved
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
import com.raydose.netshield.net.ParsedFsyFrame
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
    private val linkRouter: ProbeLinkRouter,
    private val serialSender: ((ByteArray) -> Unit)? = null,
    private val isTelemetryOnline: (probeId: String) -> Boolean = { false },
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
    private val frameStatsByProbe = ConcurrentHashMap<String, TcpFrameStats>()
    /** 任意 0x23（实时/阈值/5min/其它）收到时刷新，用于僵死连接判定 */
    private val lastRx23Ms = ConcurrentHashMap<String, Long>()
    private val tcpConnectedAtMs = ConcurrentHashMap<String, Long>()
    /** 组播发现日志去重：同一设备 60s 内只打一条 */
    private val lastDiscoveryLogMs = ConcurrentHashMap<String, Long>()

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

    @Volatile
    private var statsReporterRunning = false

    private var statsReporterThread: Thread? = null

    private val discovery = FsyMulticastDiscovery(
        appContext = appContext,
        onDatagram = { text ->
            onDiscoveredRaw(text)
            parseFsyBroadcast(text)?.let { broadcast ->
                maybeLogDiscovery(broadcast)
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
        startFrameStatsReporter()
        Log.i(TAG, "组播发现已启动 iface=$preferredInterfaceName")
    }

    fun stopDiscovery() {
        watchdogRunning = false
        watchdogThread?.interrupt()
        watchdogThread = null
        statsReporterRunning = false
        statsReporterThread?.interrupt()
        statsReporterThread = null
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
     * 进入探头管理时按需读取（Neiji reg 50/52/82/122/123）。
     * 应答经 [onTcpFrame] 或串口 [onProbeFrame] 回调，由 ViewModel 合并到草稿。
     * 读请求走该探头最后一条实时 0x23 的来源通道。
     */
    fun fetchManageConfig(probe: SavedProbe) {
        when (linkRouter.routeFor(probe.id)) {
            ProbeCommandLink.SERIAL -> fetchManageConfigSerial(probe)
            ProbeCommandLink.NETWORK -> fetchManageConfigTcp(probe)
            null -> when {
                probe.ip.isBlank() && isTelemetryOnline(probe.id) ->
                    fetchManageConfigSerial(probe)
                isTcpOnline(probe.id) ->
                    fetchManageConfigTcp(probe)
                isTelemetryOnline(probe.id) ->
                    fetchManageConfigSerial(probe)
                else ->
                    Log.i(TAG, "跳过读配置 ${probe.displayName}：尚无 0x23 路由")
            }
        }
    }

    private fun manageConfigReadFrames(probe: SavedProbe): List<ByteArray> {
        val addr = probe.modbusDeviceAddr()
        return listOf(
            buildReadRegsFrame(NeijiProbeRegs.DOSE_HI_TH, NeijiProbeRegs.U32_REG_COUNT, addr),
            buildReadRegsFrame(NeijiProbeRegs.DOSE_LO_TH, NeijiProbeRegs.U32_REG_COUNT, addr),
            buildReadRegsFrame(NeijiProbeRegs.ALARM_ENABLE, NeijiProbeRegs.U32_REG_COUNT, addr),
            buildReadRegsFrame(NeijiProbeRegs.ALARM_VOLUME, 1, addr),
            buildReadRegsFrame(NeijiProbeRegs.CONTROL_BIT2, NeijiProbeRegs.U32_REG_COUNT, addr),
        )
    }

    private fun fetchManageConfigTcp(probe: SavedProbe) {
        if (!isProbeOnline(probe.id)) {
            Log.i(TAG, "跳过读配置 ${probe.displayName}：TCP 未在线")
            return
        }
        val client = clients[probe.id]
        if (client == null) {
            Log.w(TAG, "跳过读配置 ${probe.displayName}：无 TCP 客户端")
            return
        }
        val reads = manageConfigReadFrames(probe)
        thread(name = "fsy-fetch-cfg-tcp-${probe.id}") {
            reads.forEach { frame ->
                if (!isProbeOnline(probe.id)) return@thread
                client.send(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
            Log.i(TAG, "已请求(TCP) ${probe.displayName} 配置 reg50/52/82/122/123")
            onLog("已请求 ${probe.displayName} 配置 reg50/52/82/122/123")
        }
    }

    private fun fetchManageConfigSerial(probe: SavedProbe) {
        val sender = serialSender
        if (sender == null) {
            Log.w(TAG, "跳过读配置 ${probe.displayName}：串口未就绪")
            return
        }
        if (!isTelemetryOnline(probe.id)) {
            Log.i(TAG, "跳过读配置 ${probe.displayName}：串口路径无近期 0x23")
            return
        }
        val reads = manageConfigReadFrames(probe)
        thread(name = "fsy-fetch-cfg-serial-${probe.id}") {
            reads.forEach { frame ->
                if (!isTelemetryOnline(probe.id)) return@thread
                sender(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
            Log.i(TAG, "已请求(串口) ${probe.displayName} 配置 reg50/52/82/122/123")
            onLog("已请求 ${probe.displayName} 配置 reg50/52/82/122/123")
        }
    }

    fun sendFrames(probeId: String, frames: List<ByteArray>) {
        if (frames.isEmpty()) return
        when (linkRouter.routeFor(probeId)) {
            ProbeCommandLink.SERIAL -> sendFramesSerial(probeId, frames)
            ProbeCommandLink.NETWORK -> sendFramesTcp(probeId, frames)
            null -> Log.i(TAG, "跳过写入 probe=$probeId：尚无 0x23 路由")
        }
    }

    private fun sendFramesSerial(probeId: String, frames: List<ByteArray>) {
        val sender = serialSender
        if (sender == null) {
            Log.w(TAG, "跳过串口写入 probe=$probeId：串口未就绪")
            return
        }
        if (!isTelemetryOnline(probeId)) {
            Log.i(TAG, "跳过串口写入 probe=$probeId：无近期 0x23")
            return
        }
        thread(name = "fsy-write-serial-$probeId") {
            frames.forEach { frame ->
                if (!isTelemetryOnline(probeId)) return@thread
                sender(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
            Log.d(TAG, "串口写入完成 probe=$probeId frames=${frames.size}")
        }
    }

    private fun sendFramesTcp(probeId: String, frames: List<ByteArray>) {
        if (!isProbeOnline(probeId)) return
        val client = clients[probeId] ?: return
        thread(name = "fsy-write-tcp-$probeId") {
            frames.forEach { frame ->
                if (!isProbeOnline(probeId)) return@thread
                client.send(frame)
                try {
                    Thread.sleep(CONFIG_READ_GAP_MS)
                } catch (_: InterruptedException) {
                    return@thread
                }
            }
            Log.d(TAG, "TCP 写入完成 probe=$probeId frames=${frames.size}")
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

    fun isTcpOnline(probeId: String): Boolean = isProbeOnline(probeId)

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
        if (probe.ip.isBlank()) {
            Log.i(TAG, "跳过 TCP 连接 ${probe.displayName}：无 IP（串口/CAN 探头）")
            return
        }
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
                    val parsed = parseFsyTcpFrame(frame)
                    if (parsed == null) {
                        statsForProbe(probe.id).parseFail.incrementAndGet()
                        logTcpRxParseFail(probe, frame)
                        return@forEach
                    }
                    logTcpRxFrame(probe, frame, parsed)
                    recordFrameStats(probe.id, parsed)
                    onTcpFrame(probe.id, parsed)
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
            val now = System.currentTimeMillis()
            tcpConnectedAtMs[probe.id] = now
            lastRx23Ms[probe.id] = now
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

    private fun maybeLogDiscovery(broadcast: FsyBroadcast) {
        val key = "${broadcast.ip}:${broadcast.protoAddr}"
        val now = System.currentTimeMillis()
        val last = lastDiscoveryLogMs[key] ?: 0L
        if (now - last < DISCOVERY_LOG_DEBOUNCE_MS) return
        lastDiscoveryLogMs[key] = now
        Log.d(TAG, "发现 ${broadcast.model} ${broadcast.ip} id=${broadcast.protoAddr}")
    }

    /** 组播再次发现已保存探头：离线则重连；在线但 0x23 已停（内机重启等）也重连 */
    private fun tryReconnectOnDiscovery(broadcast: FsyBroadcast) {
        if (suppressAutoReconnect) return
        val device = DiscoveredDevice.fromBroadcast(broadcast)
        val probe = savedProbesRef.get().firstOrNull { matchesSaved(it, device) } ?: return

        val now = System.currentTimeMillis()
        val online = isProbeOnline(probe.id)
        if (online) {
            val lastRx = lastRx23Ms[probe.id] ?: tcpConnectedAtMs[probe.id] ?: return
            if (now - lastRx < DISCOVERY_STALE_RECONNECT_MS) return
            Log.i(
                TAG,
                "组播活跃但 ${now - lastRx}ms 无 0x23，软重连 ${probe.displayName}（不断开 UI）",
            )
        } else {
            val lastDiscovery = lastDiscoveryReconnectMs[probe.id] ?: 0L
            if (now - lastDiscovery < DISCOVERY_RECONNECT_DEBOUNCE_MS) return
            val lastAttempt = lastConnectAttemptMs[probe.id] ?: 0L
            if (now - lastAttempt < CONNECT_COOLDOWN_MS) return
            Log.i(TAG, "组播发现离线探头 ${probe.displayName}，触发重连")
        }

        lastDiscoveryReconnectMs[probe.id] = now
        resetRetryDelay(probe.id)
        if (online) {
            reconnectTcpSoft(probe)
        } else {
            scheduleConnect(probe)
        }
    }

    /** 在线态 TCP 僵死/超时：重建连接但保持 UI 在线，避免剂量闪 --- */
    private fun reconnectTcpSoft(probe: SavedProbe) {
        closeClientOnly(probe.id)
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
                        } else if (isProbeOnline(probe.id)) {
                            checkTelemetryStale(probe)
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
        frameStatsByProbe.remove(probeId)
        lastRx23Ms.remove(probeId)
        tcpConnectedAtMs.remove(probeId)
    }

    private fun statsForProbe(probeId: String): TcpFrameStats =
        frameStatsByProbe.getOrPut(probeId) { TcpFrameStats() }

    private fun logTcpRxFrame(probe: SavedProbe, raw: ByteArray, parsed: ParsedFsyFrame) {
        val crcTag = if (parsed.crcOk) "OK" else "BAD"
        Log.d(
            TAG,
            "[TCP RX] ${probe.displayName}@${probe.ip} len=${raw.size} " +
                "addr=0x${parsed.addr.toString(16)} func=0x${parsed.func.toString(16).uppercase()} " +
                "crc=$crcTag | ${parsed.summary} | hex=${formatBytesHex(raw)}",
        )
    }

    private fun logTcpRxParseFail(probe: SavedProbe, raw: ByteArray) {
        Log.w(
            TAG,
            "[TCP RX] ${probe.displayName}@${probe.ip} 解析失败 len=${raw.size} hex=${formatBytesHex(raw)}",
        )
    }

    private fun formatBytesHex(data: ByteArray, maxBytes: Int = 64): String {
        val n = minOf(data.size, maxBytes)
        val hex = data.take(n).joinToString(" ") { "%02X".format(it.toUByte().toInt()) }
        return if (data.size > maxBytes) "$hex ...(+${data.size - maxBytes})" else hex
    }

    private fun recordFrameStats(probeId: String, parsed: com.raydose.netshield.net.ParsedFsyFrame) {
        val stats = statsForProbe(probeId)
        when (parsed.func) {
            0x23 -> {
                when {
                    parsed.uploadValues != null && parsed.uploadValues.size >= 8 -> {
                        stats.rx23Realtime.incrementAndGet()
                        stats.lastDoseX100.set(parsed.uploadValues[0])
                    }
                    parsed.thresholdValues != null -> stats.rx23Threshold.incrementAndGet()
                    parsed.fiveMinUpload != null -> stats.rx23FiveMin.incrementAndGet()
                    else -> stats.rx23Other.incrementAndGet()
                }
                lastRx23Ms[probeId] = System.currentTimeMillis()
            }
            0x13 -> stats.rx13.incrementAndGet()
            else -> stats.rxOther.incrementAndGet()
        }
    }

    private fun startFrameStatsReporter() {
        if (statsReporterRunning) return
        statsReporterRunning = true
        statsReporterThread = thread(name = "fsy-tcp-stats", isDaemon = true) {
            while (statsReporterRunning) {
                try {
                    Thread.sleep(FRAME_STATS_INTERVAL_MS)
                    logFrameStatsWindow()
                } catch (_: InterruptedException) {
                    break
                }
            }
        }
    }

    /** TCP 仍标记在线但长时间无任何 0x23 → 视为僵死连接并重连 */
    private fun checkTelemetryStale(probe: SavedProbe) {
        if (clients[probe.id] == null) return
        val now = System.currentTimeMillis()
        val lastRx = lastRx23Ms[probe.id]
            ?: tcpConnectedAtMs[probe.id]
            ?: return
        if (now - lastRx < TELEMETRY_STALE_MS) return
        Log.w(
            TAG,
            "0x23 超时 ${now - lastRx}ms 无 0x23，软重连 ${probe.displayName}@${probe.ip}",
        )
        reconnectTcpSoft(probe)
    }

    /** 每 10s 汇总上一窗口内各探头收到的 0x23/0x13 等帧数，便于排查卡片读数不刷新 */
    private fun logFrameStatsWindow() {
        val probes = savedProbesRef.get()
        if (probes.isEmpty()) return

        probes.forEach { probe ->
            val stats = frameStatsByProbe[probe.id]
            val hasClient = clients[probe.id] != null
            val online = isProbeOnline(probe.id) && hasClient
            val rx23Realtime = stats?.rx23Realtime?.getAndSet(0) ?: 0L
            val rx23Threshold = stats?.rx23Threshold?.getAndSet(0) ?: 0L
            val rx23FiveMin = stats?.rx23FiveMin?.getAndSet(0) ?: 0L
            val rx23Other = stats?.rx23Other?.getAndSet(0) ?: 0L
            val rx13 = stats?.rx13?.getAndSet(0) ?: 0L
            val rxOther = stats?.rxOther?.getAndSet(0) ?: 0L
            val parseFail = stats?.parseFail?.getAndSet(0) ?: 0L
            val rx23Total = rx23Realtime + rx23Threshold + rx23FiveMin + rx23Other
            val doseX100 = stats?.lastDoseX100?.get() ?: -1L
            val doseText = if (doseX100 >= 0) "%.2f uSv/h".format(doseX100 / 100.0) else "—"
            val lastRx = lastRx23Ms[probe.id]
            val staleSec = if (online && lastRx != null) {
                ((System.currentTimeMillis() - lastRx) / 1000L).coerceAtLeast(0L)
            } else {
                null
            }

            val line = buildString {
                append("[0x23统计 10s] ${probe.displayName}@${probe.ip} ")
                append("tcp=${if (online) "在线" else if (isProbeOnline(probe.id) && !hasClient) "半开" else "离线"} ")
                append("0x23合计=$rx23Total ")
                append("(实时=$rx23Realtime 阈值=$rx23Threshold 5min=$rx23FiveMin 其它=$rx23Other) ")
                append("0x13=$rx13 其它功能=$rxOther 解析失败=$parseFail ")
                append("末帧剂量=$doseText")
                if (staleSec != null && rx23Realtime == 0L) {
                    append(" 距上次实时=${staleSec}s")
                }
            }
            if (rx23Total == 0L && online) {
                Log.w(TAG, line)
            } else {
                Log.d(TAG, line)
            }
        }
    }

    private class TcpFrameStats {
        val rx23Realtime = AtomicLong(0)
        val rx23Threshold = AtomicLong(0)
        val rx23FiveMin = AtomicLong(0)
        val rx23Other = AtomicLong(0)
        val rx13 = AtomicLong(0)
        val rxOther = AtomicLong(0)
        val parseFail = AtomicLong(0)
        val lastDoseX100 = AtomicLong(-1)
    }

    companion object {
        const val TAG = "NetShield"
        private const val INITIAL_RETRY_MS = 3_000L
        private const val BOOTING_RETRY_MS = 10_000L
        private const val MAX_RETRY_MS = 30_000L
        private const val DISCOVERY_RECONNECT_DEBOUNCE_MS = 4_000L
        /** 仍标记在线但超过此间隔无 0x23，组播再次出现时软重连（DHCP 续租常暂停 TCP ~10s） */
        private const val DISCOVERY_STALE_RECONNECT_MS = 20_000L
        private const val CONNECT_COOLDOWN_MS = 5_000L
        private const val WATCHDOG_INTERVAL_MS = 20_000L
        private const val WATCHDOG_MIN_GAP_MS = 18_000L
        private const val CONFIG_READ_GAP_MS = 100L
        private const val FRAME_STATS_INTERVAL_MS = 10_000L
        private const val DISCOVERY_LOG_DEBOUNCE_MS = 60_000L
        /** 内机约 1s 一帧 0x23；超过此间隔无任何 0x23 则重连 */
        private const val TELEMETRY_STALE_MS = 25_000L
    }
}
