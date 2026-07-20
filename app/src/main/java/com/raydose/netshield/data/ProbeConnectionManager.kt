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
import com.raydose.netshield.net.ParsedFsyFrame
import com.raydose.netshield.net.buildOtaDataFrame
import com.raydose.netshield.net.buildOtaDoneFrame
import com.raydose.netshield.net.buildOtaStartFrame
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.matchesManageConfigRead
import com.raydose.netshield.net.matchesWriteAck
import com.raydose.netshield.net.otaCrc32
import com.raydose.netshield.net.parseWriteAckExpectation
import com.raydose.netshield.net.findRoutesToHost
import com.raydose.netshield.net.listFsyNetworkOptions
import com.raydose.netshield.net.pickFsyNetworkForMulticast
import com.raydose.netshield.net.parseFsyBroadcast
import com.raydose.netshield.net.parseFsyTcpFrame
import com.raydose.netshield.net.pickBestRoute
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull
import java.util.concurrent.Executors
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

data class ManageConfigFetchResult(
    val probeId: String,
    val okRegs: List<Int>,
    val missingRegs: List<Int>,
) {
    val complete: Boolean get() = missingRegs.isEmpty()
}

data class ManageWriteResult(
    val probeId: String,
    val okCount: Int,
    val failCount: Int,
) {
    val complete: Boolean get() = failCount == 0
}

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
    /** OTA 期间固件静默 0x23，禁止因此断 TCP / 判离线 */
    private val otaActiveProbeIds = ConcurrentHashMap.newKeySet<String>()

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

    private data class ConfigReadEvent(val probeId: String, val frame: ParsedFsyFrame)

    private val configReadChannel = Channel<ConfigReadEvent>(Channel.UNLIMITED)

    @Volatile
    private var configReadSessionProbeId: String? = null

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
     * 进入探头管理时按需读取（Neiji reg 50/52/82/98/122/123）。
     * 按寄存器发 0x03 → 等 0x13 应答 → 超时重试（最多 3 次）。
     */
    suspend fun fetchManageConfigWithRetry(probe: SavedProbe): ManageConfigFetchResult {
        val route = resolveConfigReadRoute(probe)
        if (route == null) {
            Log.i(TAG, "跳过读配置 ${probe.displayName}：尚无 0x23 路由")
            return ManageConfigFetchResult(probe.id, emptyList(), manageConfigRegList())
        }
        if (!isProbeReadyForConfigRead(probe, route)) {
            Log.i(TAG, "跳过读配置 ${probe.displayName}：探头离线")
            return ManageConfigFetchResult(probe.id, emptyList(), manageConfigRegList())
        }

        val deviceAddr = probe.modbusDeviceAddr().toUByte().toInt()
        val specs = manageConfigReadSpecs(probe)
        val okRegs = mutableListOf<Int>()
        val missingRegs = mutableListOf<Int>()

        configReadSessionProbeId = probe.id
        drainConfigReadChannel()
        try {
            for ((index, spec) in specs.withIndex()) {
                if (!isProbeReadyForConfigRead(probe, route)) break

                var acked = false
                for (attempt in 1..CONFIG_READ_MAX_ATTEMPTS) {
                    if (!isProbeReadyForConfigRead(probe, route)) break
                    if (!sendConfigReadFrame(probe, route, spec.frame)) break

                    val ack = withTimeoutOrNull(CONFIG_READ_PER_REG_TIMEOUT_MS) {
                        while (true) {
                            val event = configReadChannel.receive()
                            if (event.probeId != probe.id) continue
                            if (event.frame.matchesManageConfigRead(spec.reg, deviceAddr)) {
                                return@withTimeoutOrNull event.frame
                            }
                        }
                        @Suppress("UNREACHABLE_CODE")
                        null
                    }

                    if (ack != null) {
                        okRegs.add(spec.reg)
                        acked = true
                        break
                    }
                    if (attempt < CONFIG_READ_MAX_ATTEMPTS) {
                        delay(CONFIG_READ_RETRY_GAP_MS)
                    }
                }

                if (!acked) {
                    missingRegs.add(spec.reg)
                    Log.w(
                        TAG,
                        "读配置超时 probe=${probe.displayName} reg=0x${spec.reg.toString(16).uppercase()} " +
                            "attempts=$CONFIG_READ_MAX_ATTEMPTS",
                    )
                } else if (index + 1 < specs.size) {
                    delay(CONFIG_READ_INTER_REG_GAP_MS)
                }
            }
        } finally {
            configReadSessionProbeId = null
            drainConfigReadChannel()
        }

        val routeLabel = if (route == ProbeCommandLink.SERIAL) "串口" else "TCP"
        if (missingRegs.isEmpty()) {
            Log.i(TAG, "读配置完成($routeLabel) ${probe.displayName} reg50/52/82/98/122/123")
            onLog("已读取 ${probe.displayName} 配置")
        } else {
            val miss = missingRegs.joinToString { "0x${it.toString(16).uppercase()}" }
            Log.w(TAG, "读配置部分失败($routeLabel) ${probe.displayName} 未完成: $miss")
            onLog("读配置部分未完成($miss)")
        }

        return ManageConfigFetchResult(probe.id, okRegs.toList(), missingRegs.toList())
    }

    /** 读写配置会话中由 MainViewModel 转发 0x13 / 0x16 / 0x20 应答 */
    fun offerConfigReadFrame(probeId: String, frame: ParsedFsyFrame) {
        if (configReadSessionProbeId == null) return
        when (frame.func) {
            0x13, 0x16, 0x20 -> configReadChannel.trySend(ConfigReadEvent(probeId, frame))
        }
    }

    /**
     * 按帧发写请求 → 等 0x16 / 0x20 应答 → 超时重试（最多 3 次）。
     * 含非 0x06/0x10 写帧时退化为 [sendFrames] 连发。
     */
    suspend fun sendFramesWithRetry(
        probe: SavedProbe,
        frames: List<ByteArray>,
        logLabel: String = "写配置",
    ): ManageWriteResult {
        if (frames.isEmpty()) return ManageWriteResult(probe.id, 0, 0)

        val route = resolveConfigReadRoute(probe)
        if (route == null) {
            Log.i(TAG, "跳过$logLabel ${probe.displayName}：尚无 0x23 路由")
            return ManageWriteResult(probe.id, 0, frames.size)
        }
        if (!isProbeReadyForConfigRead(probe, route)) {
            Log.i(TAG, "跳过$logLabel ${probe.displayName}：探头离线")
            return ManageWriteResult(probe.id, 0, frames.size)
        }

        val expectations = frames.map { parseWriteAckExpectation(it) }
        if (expectations.any { it == null }) {
            Log.w(TAG, "$logLabel 含非标准写帧，退化为连发 probe=${probe.displayName}")
            sendFrames(probe.id, frames)
            return ManageWriteResult(probe.id, frames.size, 0)
        }

        var okCount = 0
        var failCount = 0

        configReadSessionProbeId = probe.id
        drainConfigReadChannel()
        try {
            for ((index, frame) in frames.withIndex()) {
                val expect = expectations[index]!!
                if (!isProbeReadyForConfigRead(probe, route)) {
                    failCount = frames.size - index
                    break
                }

                var acked = false
                for (attempt in 1..CONFIG_WRITE_MAX_ATTEMPTS) {
                    if (!isProbeReadyForConfigRead(probe, route)) break
                    if (!sendConfigReadFrame(probe, route, frame)) break

                    val ack = withTimeoutOrNull(CONFIG_WRITE_PER_FRAME_TIMEOUT_MS) {
                        while (true) {
                            val event = configReadChannel.receive()
                            if (event.probeId != probe.id) continue
                            if (event.frame.matchesWriteAck(expect)) {
                                return@withTimeoutOrNull event.frame
                            }
                        }
                        @Suppress("UNREACHABLE_CODE")
                        null
                    }

                    if (ack != null) {
                        okCount++
                        acked = true
                        break
                    }
                    if (attempt < CONFIG_WRITE_MAX_ATTEMPTS) {
                        delay(CONFIG_WRITE_RETRY_GAP_MS)
                    }
                }

                if (!acked) {
                    failCount = frames.size - index
                    Log.w(
                        TAG,
                        "$logLabel 超时 probe=${probe.displayName} reg=0x${expect.reg.toString(16).uppercase()} " +
                            "attempts=$CONFIG_WRITE_MAX_ATTEMPTS",
                    )
                    break
                }
                if (index + 1 < frames.size) {
                    delay(CONFIG_WRITE_INTER_FRAME_GAP_MS)
                }
            }
        } finally {
            configReadSessionProbeId = null
            drainConfigReadChannel()
        }

        val routeLabel = if (route == ProbeCommandLink.SERIAL) "串口" else "TCP"
        if (failCount == 0) {
            Log.i(TAG, "$logLabel 完成($routeLabel) ${probe.displayName} frames=${frames.size}")
            onLog("已写入 ${probe.displayName} 配置")
        } else {
            Log.w(TAG, "$logLabel 部分失败($routeLabel) ${probe.displayName} ok=$okCount fail=$failCount")
            onLog("写配置部分失败($okCount/${frames.size})")
        }

        return ManageWriteResult(probe.id, okCount, failCount)
    }

    /**
     * 探头固件 OTA：START → DATA(≤128B) → DONE。
     * DATA/DONE 禁止重试；START 可短重试。OTA 期间暂停 0x23 僵死断线。
     */
    suspend fun upgradeFirmware(
        probe: SavedProbe,
        fileBytes: ByteArray,
        onProgress: (ZjbOtaProgress) -> Unit,
    ): Result<Unit> = runCatching {
        if (!NeijiFirmwareRules.isValidPayload(fileBytes.size.toLong())) {
            error(NeijiFirmwareRules.REJECT_MESSAGE)
        }

        val route = resolveConfigReadRoute(probe)
            ?: error("探头离线，无法升级固件")
        if (!isProbeReadyForConfigRead(probe, route)) {
            error("探头离线，无法升级固件")
        }

        val addr = probe.modbusDeviceAddr()
        val crc32 = otaCrc32(fileBytes)
        val totalChunks = (fileBytes.size + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE

        otaActiveProbeIds.add(probe.id)
        configReadSessionProbeId = probe.id
        drainConfigReadChannel()
        try {
            onProgress(ZjbOtaProgress("发送 OTA_START", 0f))
            sendOtaWrite(
                probe = probe,
                route = route,
                frame = buildOtaStartFrame(fileBytes.size, addr),
                reg = REG_OTA_START,
                timeoutMs = OTA_START_TIMEOUT_MS,
                maxAttempts = OTA_START_MAX_ATTEMPTS,
                label = "OTA_START",
            )

            var offset = 0
            var chunkIndex = 0
            while (offset < fileBytes.size) {
                val end = minOf(offset + OTA_CHUNK_SIZE, fileBytes.size)
                var chunk = fileBytes.copyOfRange(offset, end)
                if (chunk.size % 2 != 0) {
                    chunk = chunk + byteArrayOf(0xFF.toByte())
                }
                chunkIndex++
                onProgress(
                    ZjbOtaProgress(
                        statusText = "发送固件 $chunkIndex/$totalChunks",
                        progress = offset.toFloat() / fileBytes.size.toFloat(),
                        deviceWritten = offset.toLong(),
                    ),
                )
                sendOtaWrite(
                    probe = probe,
                    route = route,
                    frame = buildOtaDataFrame(chunk, addr),
                    reg = REG_OTA_DATA,
                    timeoutMs = OTA_DATA_TIMEOUT_MS,
                    maxAttempts = 1,
                    label = "OTA_DATA#$chunkIndex",
                )
                /* 给内机写 Flash / 轮询 W5500 留间隙，降低 TCP 会话被拆风险 */
                if (route == ProbeCommandLink.NETWORK) {
                    delay(OTA_TCP_INTER_CHUNK_GAP_MS)
                }
                offset = end
            }

            onProgress(ZjbOtaProgress("发送 OTA_DONE", 1f, deviceWritten = fileBytes.size.toLong()))
            sendOtaWrite(
                probe = probe,
                route = route,
                frame = buildOtaDoneFrame(crc32, addr),
                reg = REG_OTA_DONE,
                timeoutMs = OTA_DONE_TIMEOUT_MS,
                maxAttempts = 1,
                label = "OTA_DONE",
            )

            onProgress(
                ZjbOtaProgress(
                    statusText = "固件已发送，探头将校验并重启",
                    progress = 1f,
                    deviceWritten = fileBytes.size.toLong(),
                ),
            )
            val routeLabel = if (route == ProbeCommandLink.SERIAL) "串口" else "TCP"
            Log.i(TAG, "OTA 完成($routeLabel) ${probe.displayName} size=${fileBytes.size}")
            onLog("已推送 ${probe.displayName} 固件")
        } finally {
            configReadSessionProbeId = null
            drainConfigReadChannel()
            otaActiveProbeIds.remove(probe.id)
        }
    }

    private suspend fun sendOtaWrite(
        probe: SavedProbe,
        route: ProbeCommandLink,
        frame: ByteArray,
        reg: Int,
        timeoutMs: Long,
        maxAttempts: Int,
        label: String,
    ) {
        val expect = parseWriteAckExpectation(frame)
            ?: error("$label 帧格式错误")
        var lastError: String? = null
        for (attempt in 1..maxAttempts) {
            if (!isProbeReadyForOta(probe, route)) {
                error("探头连接中断（$label）")
            }
            if (!sendConfigReadFrame(probe, route, frame)) {
                error("发送失败（$label）")
            }
            val ack = withTimeoutOrNull(timeoutMs) {
                while (true) {
                    val event = configReadChannel.receive()
                    if (event.probeId != probe.id) continue
                    if (event.frame.matchesWriteAck(expect)) {
                        return@withTimeoutOrNull event.frame
                    }
                }
                @Suppress("UNREACHABLE_CODE")
                null
            }
            if (ack != null) return
            lastError = "$label 无应答"
            if (attempt < maxAttempts) {
                delay(OTA_RETRY_GAP_MS)
            }
        }
        error(lastError ?: "$label 失败")
    }

    private fun isProbeReadyForOta(probe: SavedProbe, route: ProbeCommandLink): Boolean {
        if (probe.id !in otaActiveProbeIds) {
            return isProbeReadyForConfigRead(probe, route)
        }
        return when (route) {
            ProbeCommandLink.SERIAL -> serialSender != null
            ProbeCommandLink.NETWORK -> clients[probe.id] != null
        }
    }

    /** @deprecated 使用 [fetchManageConfigWithRetry]；保留给旧调用方 */
    fun fetchManageConfig(probe: SavedProbe) {
        thread(name = "fsy-fetch-cfg-${probe.id}") {
            kotlinx.coroutines.runBlocking {
                fetchManageConfigWithRetry(probe)
            }
        }
    }

    private data class ManageConfigReadSpec(val reg: Int, val frame: ByteArray)

    private fun manageConfigRegList(): List<Int> = listOf(
        NeijiProbeRegs.DOSE_HI_TH,
        NeijiProbeRegs.DOSE_LO_TH,
        NeijiProbeRegs.ALARM_ENABLE,
        NeijiProbeRegs.SOFTWARE_VERSION,
        NeijiProbeRegs.ALARM_VOLUME,
        NeijiProbeRegs.CONTROL_BIT2,
    )

    private fun manageConfigReadSpecs(probe: SavedProbe): List<ManageConfigReadSpec> {
        val addr = probe.modbusDeviceAddr()
        return manageConfigRegList().map { reg ->
            val count = when (reg) {
                NeijiProbeRegs.ALARM_VOLUME -> 1
                NeijiProbeRegs.SOFTWARE_VERSION -> NeijiProbeRegs.SOFTWARE_VERSION_REGS
                else -> NeijiProbeRegs.U32_REG_COUNT
            }
            ManageConfigReadSpec(reg, buildReadRegsFrame(reg, count, addr))
        }
    }

    private fun resolveConfigReadRoute(probe: SavedProbe): ProbeCommandLink? {
        return when (linkRouter.routeFor(probe.id)) {
            ProbeCommandLink.SERIAL -> ProbeCommandLink.SERIAL
            ProbeCommandLink.NETWORK -> ProbeCommandLink.NETWORK
            null -> when {
                probe.ip.isBlank() && isTelemetryOnline(probe.id) -> ProbeCommandLink.SERIAL
                isTcpOnline(probe.id) -> ProbeCommandLink.NETWORK
                isTelemetryOnline(probe.id) -> ProbeCommandLink.SERIAL
                else -> null
            }
        }
    }

    private fun isProbeReadyForConfigRead(probe: SavedProbe, route: ProbeCommandLink): Boolean {
        return when (route) {
            ProbeCommandLink.SERIAL -> isTelemetryOnline(probe.id)
            ProbeCommandLink.NETWORK -> isProbeOnline(probe.id)
        }
    }

    private fun sendConfigReadFrame(
        probe: SavedProbe,
        route: ProbeCommandLink,
        frame: ByteArray,
    ): Boolean {
        return when (route) {
            ProbeCommandLink.SERIAL -> {
                val sender = serialSender
                if (sender == null) {
                    Log.w(TAG, "跳过读配置 ${probe.displayName}：串口未就绪")
                    false
                } else {
                    sender(frame)
                    true
                }
            }
            ProbeCommandLink.NETWORK -> {
                val client = clients[probe.id]
                if (client == null) {
                    Log.w(TAG, "跳过读配置 ${probe.displayName}：无 TCP 客户端")
                    false
                } else {
                    client.send(frame)
                    true
                }
            }
        }
    }

    private fun drainConfigReadChannel() {
        while (configReadChannel.tryReceive().isSuccess) {
            // discard stale 0x13
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
        if (probeId in otaActiveProbeIds) return null
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
        /** 写多帧时间隔（读配置已改为按 reg 等应答） */
        private const val CONFIG_READ_GAP_MS = 100L
        private const val CONFIG_READ_MAX_ATTEMPTS = 3
        private const val CONFIG_READ_PER_REG_TIMEOUT_MS = 500L
        private const val CONFIG_READ_RETRY_GAP_MS = 150L
        private const val CONFIG_READ_INTER_REG_GAP_MS = 80L
        private const val CONFIG_WRITE_MAX_ATTEMPTS = 3
        private const val CONFIG_WRITE_PER_FRAME_TIMEOUT_MS = 500L
        private const val CONFIG_WRITE_RETRY_GAP_MS = 150L
        private const val CONFIG_WRITE_INTER_FRAME_GAP_MS = 80L
        private const val DISCOVERY_LOG_DEBOUNCE_MS = 60_000L
        /** 8s 无 0x23 则断 TCP，等组播再连（与 UI 组播超时一致，扛 PHY 短抖） */
        private const val TELEMETRY_STALE_MS = 8_000L
        private const val OTA_CHUNK_SIZE = 128
        private const val REG_OTA_START = 0x00C8
        private const val REG_OTA_DONE = 0x00CA
        private const val REG_OTA_DATA = 0x00D0
        /** START 预擦 Download 扇区可能十余秒 */
        private const val OTA_START_TIMEOUT_MS = 45_000L
        private const val OTA_START_MAX_ATTEMPTS = 2
        private const val OTA_DATA_TIMEOUT_MS = 10_000L
        private const val OTA_DONE_TIMEOUT_MS = 20_000L
        private const val OTA_RETRY_GAP_MS = 200L
        private const val OTA_TCP_INTER_CHUNK_GAP_MS = 15L
    }
}
