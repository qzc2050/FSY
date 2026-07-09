package com.raydose.netshield.data

import android.content.Context
import android.net.Network
import android.util.Log
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.NeijiProbeRegs
import com.raydose.netshield.model.ProbeCommandLink
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.isPlausibleRealtimeDoseX100
import com.raydose.netshield.model.matchesSaved
import com.raydose.netshield.net.FsyBroadcast
import com.raydose.netshield.net.FsyProtocolFrameCollector
import com.raydose.netshield.net.FsyMulticastDiscovery
import com.raydose.netshield.model.modbusDeviceAddr
import com.raydose.netshield.net.FsyTcpClient
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.findRoutesToHost
import com.raydose.netshield.net.listFsyNetworkOptions
import com.raydose.netshield.net.pickFsyNetworkForMulticast
import com.raydose.netshield.net.parseFsyBroadcast
import com.raydose.netshield.net.parseFsyTcpFrame
import com.raydose.netshield.net.pickBestRoute
import java.util.concurrent.Executors
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

/**
 * 组播发现 + 对已保存探头维持 TCP，解析 0x23 实时数据。
 * TCP 连接前须先收到组播，确认序列号与 IP；断线后清除锚点，等下一次组播再连。
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
    private val lastDiscoveryReconnectMs = ConcurrentHashMap<String, Long>()
    private val lastConnectAttemptMs = ConcurrentHashMap<String, Long>()
    private val connectLocks = ConcurrentHashMap<String, Any>()
    /** 任意 0x23（实时/阈值/5min/其它）收到时刷新，用于僵死连接判定 */
    private val lastRx23Ms = ConcurrentHashMap<String, Long>()
    private val tcpConnectedAtMs = ConcurrentHashMap<String, Long>()
    /** 组播发现日志去重：同一设备 60s 内只打一条 */
    private val lastDiscoveryLogMs = ConcurrentHashMap<String, Long>()
    /** 组播已确认的 serial/ip，TCP 连接唯一依据 */
    private val tcpDiscoveryAnchors = ConcurrentHashMap<String, TcpDiscoveryAnchor>()

    private data class TcpDiscoveryAnchor(
        val serial: String,
        val ip: String,
        val controlPort: Int,
        val dataPort: Int,
        val seenAtMillis: Long,
    )

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

    /** 组播收包线程只做解析/回调；TCP 调度放到独立线程，避免阻塞 UDP receive */
    private val discoveryFollowUp = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "fsy-mcast-followup").apply { isDaemon = true }
    }

    private val discovery = FsyMulticastDiscovery(
        appContext = appContext,
        onDatagram = { text ->
            onDiscoveredRaw(text)
            parseFsyBroadcast(text)?.let { broadcast ->
                discoveryFollowUp.execute {
                    maybeLogDiscovery(broadcast)
                    tryReconnectOnDiscovery(broadcast)
                }
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
        val pick = pickFsyNetworkForMulticast(options)
        preferredNetwork = pick?.network
        preferredInterfaceName = pick?.interfaceName
        preferredLocalIpv4 = pick?.localIpv4
    }

    /** 打开「添加探头」等场景下刷新网卡并重启组播监听（网线后插、DHCP 变更时有用） */
    fun restartDiscovery() {
        refreshNetworkPreference()
        discovery.start(preferredNetwork, preferredInterfaceName)
        if (!watchdogRunning) startWatchdog()
        Log.i(
            TAG,
            "组播发现已重启 iface=$preferredInterfaceName ip=$preferredLocalIpv4 net=$preferredNetwork",
        )
    }

    fun startDiscovery() {
        refreshNetworkPreference()
        discovery.start(preferredNetwork, preferredInterfaceName)
        startWatchdog()
        Log.i(
            TAG,
            "组播发现已启动 iface=$preferredInterfaceName ip=$preferredLocalIpv4 net=$preferredNetwork",
        )
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
                tcpDiscoveryAnchors.remove(probe.id)
            }
            Log.i(TAG, "探头列表已刷新，等待组播确认 serial/ip 后建立 TCP")
        } finally {
            suppressAutoReconnect = false
        }
    }

    fun connect(probe: SavedProbe) {
        scheduleConnect(probe)
    }

    /** 从「添加探头」等已确认的发现项建立 TCP（写入组播锚点后连接）。 */
    fun connectAfterDiscovery(probe: SavedProbe, device: DiscoveredDevice) {
        if (!recordDiscoveryAnchor(device, probe)) {
            Log.i(TAG, "跳过 TCP 连接 ${probe.displayName}：发现项缺少 serial/ip")
            return
        }
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
        val resolved = resolveProbeForTcpConnect(probe) ?: run {
            Log.i(TAG, "跳过 TCP 连接 ${probe.displayName}：等待组播确认 serial/ip")
            return
        }
        lastConnectAttemptMs[resolved.id] = System.currentTimeMillis()
        val generation = nextConnectGeneration(resolved.id)
        val lock = connectLocks.getOrPut(resolved.id) { Any() }
        thread(name = "fsy-tcp-connect-${resolved.id}") {
            synchronized(lock) {
                if (isStaleConnect(resolved.id, generation)) return@synchronized
                connectOnWorker(resolved, generation)
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
    }

    private fun handleConnectionLost(probe: SavedProbe, reason: String) {
        resetTcpConnection(probe, reason, clearDiscoveryAnchor = true)
    }

    /** 组播已确认时重置 TCP，可选保留锚点以便立即重连 */
    private fun resetTcpConnection(
        probe: SavedProbe,
        reason: String,
        clearDiscoveryAnchor: Boolean,
    ) {
        if (!isProbeOnline(probe.id) && clients[probe.id] == null) return
        Log.i(TAG, "TCP 断开 ${probe.displayName}@${probe.ip}: $reason")
        closeClientOnly(probe.id)
        setProbeOnline(probe.id, false)
        if (clearDiscoveryAnchor) {
            tcpDiscoveryAnchors.remove(probe.id)
            Log.i(TAG, "已清除 ${probe.displayName} 组播锚点，等待组播再次确认 serial/ip")
        } else {
            Log.i(TAG, "保留 ${probe.displayName} 组播锚点，立即重连 TCP")
        }
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
        lateinit var client: FsyTcpClient
        client = FsyTcpClient(
            appContext = appContext,
            onReceive = { chunk ->
                val frames = collector.feed(chunk)
                frames.forEach { frame ->
                    val parsed = parseFsyTcpFrame(frame) ?: return@forEach
                    if (!recordRx23AndShouldDispatch(probe.id, parsed)) return@forEach
                    onTcpFrame(probe.id, parsed)
                }
            },
            onError = { msg ->
                Log.w(TAG, "TCP ${probe.displayName}@${probe.ip}: $msg")
                if (clients[probe.id] === client) {
                    handleConnectionLost(probe, msg)
                }
            },
            onRemoteDisconnected = {
                if (clients[probe.id] === client) {
                    handleConnectionLost(probe, "对端关闭连接")
                }
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
            client.beginReading()
            val now = System.currentTimeMillis()
            tcpConnectedAtMs[probe.id] = now
            Log.i(TAG, "TCP 已建立 ${probe.displayName} ${probe.ip}:${probe.controlPort}，等待 0x23")
        } else {
            setProbeOnline(probe.id, false)
            tcpDiscoveryAnchors.remove(probe.id)
            Log.w(TAG, "TCP 连接失败 ${probe.ip}:${probe.controlPort}，等待组播再次确认 serial/ip")
        }
    }

    private fun maybeLogDiscovery(broadcast: FsyBroadcast) {
        val key = "${broadcast.ip}:${broadcast.protoAddr}"
        val now = System.currentTimeMillis()
        val last = lastDiscoveryLogMs[key] ?: 0L
        if (now - last < DISCOVERY_LOG_DEBOUNCE_MS) return
        lastDiscoveryLogMs[key] = now
        Log.d(TAG, "发现 ${broadcast.model} ${broadcast.ip} id=${broadcast.protoAddr}")
    }

    /** 组播再次发现已保存探头：确认 serial/ip 后建立或重建 TCP（0x23 超时由组播触发重连） */
    private fun tryReconnectOnDiscovery(broadcast: FsyBroadcast) {
        if (suppressAutoReconnect) return
        val device = DiscoveredDevice.fromBroadcast(broadcast)
        val probe = savedProbesRef.get().firstOrNull { matchesSaved(it, device) } ?: return
        if (!recordDiscoveryAnchor(device, probe)) return
        val resolved = resolveProbeForTcpConnect(probe) ?: return

        val now = System.currentTimeMillis()
        val staleReason = tcpStaleReconnectReason(resolved.id, now)
        if (staleReason != null) {
            Log.i(TAG, "${staleReason.logLabel}，组播触发重连 ${resolved.displayName}")
            lastDiscoveryReconnectMs[resolved.id] = now
            resetTcpConnection(
                probe = resolved,
                reason = staleReason.disconnectReason,
                clearDiscoveryAnchor = false,
            )
            scheduleConnect(resolved)
            return
        }
        if (clients[resolved.id] != null) {
            return
        }

        val lastDiscovery = lastDiscoveryReconnectMs[resolved.id] ?: 0L
        if (now - lastDiscovery < DISCOVERY_RECONNECT_DEBOUNCE_MS) return
        val lastAttempt = lastConnectAttemptMs[resolved.id] ?: 0L
        if (now - lastAttempt < CONNECT_ATTEMPT_GUARD_MS) return
        Log.i(
            TAG,
            "组播确认 ${resolved.serial}@${resolved.ip}，建立 TCP ${resolved.displayName}",
        )
        lastDiscoveryReconnectMs[resolved.id] = now
        scheduleConnect(resolved)
    }

    private enum class TcpStaleReconnectReason(
        val disconnectReason: String,
        val logLabel: String,
    ) {
        FIRST_RX23_TIMEOUT(
            disconnectReason = "TCP 已建立但首帧 0x23 超时",
            logLabel = "首帧 0x23 超时",
        ),
        RX23_STALE(
            disconnectReason = "0x23 超时无数据",
            logLabel = "0x23 超时",
        ),
    }

    /** TCP 已连但 0x23 超时 → 返回重连原因；连接正常或仍在等待首帧则 null */
    private fun tcpStaleReconnectReason(probeId: String, now: Long): TcpStaleReconnectReason? {
        if (clients[probeId] == null) return null
        val connectedAt = tcpConnectedAtMs[probeId] ?: return null
        val lastRx = lastRx23Ms[probeId] ?: 0L
        if (!isProbeOnline(probeId)) {
            val base = maxOf(connectedAt, lastRx)
            if (now - base < FIRST_RX23_WAIT_MS) return null
            return TcpStaleReconnectReason.FIRST_RX23_TIMEOUT
        }
        val base = if (lastRx > 0L) lastRx else connectedAt
        if (now - base < TELEMETRY_STALE_MS) return null
        return TcpStaleReconnectReason.RX23_STALE
    }

    private fun recordDiscoveryAnchor(source: DiscoveredDevice, probe: SavedProbe): Boolean {
        val serial = source.serial.trim()
        val ip = source.ip.trim()
        if (serial.isEmpty() || ip.isBlank()) return false
        val probeSerial = probe.serial.trim()
        if (probeSerial.isNotEmpty() && !probeSerial.equals(serial, ignoreCase = true)) {
            return false
        }
        val seenAt = System.currentTimeMillis()
        tcpDiscoveryAnchors[probe.id] = TcpDiscoveryAnchor(
            serial = serial,
            ip = ip,
            controlPort = source.controlPort,
            dataPort = source.dataPort,
            seenAtMillis = seenAt,
        )
        linkRouter.recordMulticastKeepalive(probe.id)
        return true
    }

    private fun resolveProbeForTcpConnect(probe: SavedProbe): SavedProbe? {
        if (probe.ip.isBlank()) return null
        val anchor = tcpDiscoveryAnchors[probe.id] ?: return null
        if (anchor.ip.isBlank()) return null
        val probeSerial = probe.serial.trim()
        if (probeSerial.isEmpty() || !probeSerial.equals(anchor.serial.trim(), ignoreCase = true)) {
            return null
        }
        return probe.copy(
            ip = anchor.ip,
            controlPort = anchor.controlPort.takeIf { it > 0 } ?: probe.controlPort,
            dataPort = anchor.dataPort.takeIf { it > 0 } ?: probe.dataPort,
        )
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
                        if (clients[probe.id] != null) {
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
        tcpDiscoveryAnchors.remove(probeId)
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
        lastRx23Ms.remove(probeId)
        tcpConnectedAtMs.remove(probeId)
    }

    private fun recordRx23AndShouldDispatch(
        probeId: String,
        parsed: com.raydose.netshield.net.ParsedFsyFrame,
    ): Boolean {
        if (parsed.func != 0x23) return isProbeOnline(probeId)

        val now = System.currentTimeMillis()
        lastRx23Ms[probeId] = now
        if (isProbeOnline(probeId)) return true

        val values = parsed.uploadValues
        val isRealtime =
            values != null && values.size >= 8 && isPlausibleRealtimeDoseX100(values[0])
        val isFiveMin = parsed.fiveMinUpload != null
        if (!isRealtime && !isFiveMin) {
            return false
        }

        setProbeOnline(probeId, true)
        Log.i(TAG, "TCP 在线 probe=$probeId")
        return true
    }

    /** TCP 已建立后长时间无 0x23 → 断开，等组播再次触发重连 */
    private fun checkTelemetryStale(probe: SavedProbe) {
        val now = System.currentTimeMillis()
        when (tcpStaleReconnectReason(probe.id, now)) {
            null -> Unit
            TcpStaleReconnectReason.FIRST_RX23_TIMEOUT -> {
                Log.w(
                    TAG,
                    "首帧 0x23 超时，断开 ${probe.displayName}@${probe.ip}，等待组播重连",
                )
                handleConnectionLost(probe, TcpStaleReconnectReason.FIRST_RX23_TIMEOUT.disconnectReason)
            }
            TcpStaleReconnectReason.RX23_STALE -> {
                Log.w(
                    TAG,
                    "0x23 超时，断开 ${probe.displayName}@${probe.ip}，等待组播重连",
                )
                handleConnectionLost(probe, TcpStaleReconnectReason.RX23_STALE.disconnectReason)
            }
        }
    }

    companion object {
        const val TAG = "NetShield"
        private const val DISCOVERY_RECONNECT_DEBOUNCE_MS = 4_000L
        private const val FIRST_RX23_WAIT_MS = 8_000L
        /** 上次 TCP 连接开始后此时间内不因组播再次调度（单次 connect 含多路由 8s 超时） */
        private const val CONNECT_ATTEMPT_GUARD_MS = 15_000L
        private const val WATCHDOG_INTERVAL_MS = 3_000L
        private const val CONFIG_READ_GAP_MS = 100L
        private const val DISCOVERY_LOG_DEBOUNCE_MS = 60_000L
        /** 8s 无 0x23 则断 TCP，等组播再连（与 UI 组播超时一致，扛 PHY 短抖） */
        private const val TELEMETRY_STALE_MS = 8_000L
    }
}
