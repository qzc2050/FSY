package com.raydose.netshield.ui

import android.app.Application
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.netshield.data.AlertLogRepository
import com.raydose.netshield.data.DisplaySoundController
import com.raydose.netshield.data.HostAlarmController
import com.raydose.netshield.data.HostEnvSerialRepository
import com.raydose.netshield.data.HostSettingsRepository
import com.raydose.netshield.ui.home.HomeClockFormatter
import com.raydose.netshield.data.ProbeConfigRepository
import com.raydose.netshield.data.ProbeConnectionManager
import com.raydose.netshield.data.ProbeLinkRouter
import com.raydose.netshield.data.ProbeDoseHistoryRepository
import com.raydose.netshield.data.ProbeDoseAlarmLogAggregator
import com.raydose.netshield.data.ProbeSensorOfflineLogAggregator
import com.raydose.netshield.data.ZjbOtaClient
import com.raydose.netshield.data.ZjbOtaProgress
import com.raydose.netshield.model.DisplaySoundSettings
import com.raydose.netshield.model.PAUSE_ALARM_DURATION_MS
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.isHostAlarmSuppressed
import com.raydose.netshield.model.withExpiredPauseCleared
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.model.TimeSettings
import com.raydose.netshield.net.Hlk7688WifiClient
import com.raydose.netshield.net.HostConnectivityStatus
import com.raydose.netshield.net.detectHostConnectivity
import com.raydose.netshield.net.listFsyNetworkOptions
import com.raydose.netshield.ui.settings.AboutDeviceInfo
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.TabletFormFactor
import com.raydose.netshield.model.AlertLogKind
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.sortedForAddProbeDialog
import com.raydose.netshield.model.DoorState
import com.raydose.netshield.model.HostAdapterSnapshot
import com.raydose.netshield.model.defaultHostEnvPlaceholders
import com.raydose.netshield.model.HomeUiState
import com.raydose.netshield.model.LiveProbeTelemetry
import com.raydose.netshield.model.ProbeManageDraft
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.model.SystemAlertLog
import com.raydose.netshield.model.ProbeCommandLink
import com.raydose.netshield.model.isPlausibleRealtimeDoseX100
import com.raydose.netshield.model.applyParsedFrame
import com.raydose.netshield.model.buildAlarmEnableWriteFrame
import com.raydose.netshield.model.buildDoseLowerWriteFrame
import com.raydose.netshield.model.buildDoseUpperWriteFrame
import com.raydose.netshield.model.buildVolumeWriteFrame
import com.raydose.netshield.model.buildTimeSyncWriteFrame
import com.raydose.netshield.model.buildFiveMinHistoryRequestFrame
import com.raydose.netshield.net.buildFiveMinHistoryHourChunks
import com.raydose.netshield.model.PROBE_AUTO_SYNC_STARTUP_DELAY_MS
import com.raydose.netshield.model.PROBE_TIME_SYNC_ON_5MIN_SKEW_MS
import com.raydose.netshield.model.hostTimeInvalidForProbeSyncHint
import com.raydose.netshield.model.isHostTimeValidForProbeSync
import com.raydose.netshield.model.shouldAutoSyncProbeTime
import com.raydose.netshield.model.modbusDeviceAddr
import com.raydose.netshield.model.usvTextToX100
import com.raydose.netshield.model.deriveDoorState
import com.raydose.netshield.model.matchesSaved
import com.raydose.netshield.model.mergeFromDiscovery
import com.raydose.netshield.model.mergeConfigFromTelemetry
import com.raydose.netshield.model.mergeFromTelemetry
import com.raydose.netshield.model.buildControlBit2WriteFrame
import com.raydose.netshield.model.mergeControlBit2Enables
import com.raydose.netshield.model.toManageDraft
import com.raydose.netshield.model.toSlaveProbeUi
import com.raydose.netshield.ui.settings.SettingsTab
import com.raydose.netshield.net.parseFsyBroadcast
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap
data class ProbeSettingsUiState(
    val selectedTab: SettingsTab = SettingsTab.DisplaySound,
    val selectedProbeIndex: Int = 0,
    val manageDrafts: List<ProbeManageDraft> = emptyList(),
    val draftProbes: List<SavedProbe> = emptyList(),
    val discoveredDevices: List<DiscoveredDevice> = emptyList(),
    val displaySound: DisplaySoundSettings = DisplaySoundSettings(),
    val hostNetwork: HostNetworkSettings = HostNetworkSettings(),
    val slaveNetworkCards: List<SlaveNetworkCard> = emptyList(),
    val timeSettings: TimeSettings = TimeSettings(),
    val showAddProbeDialog: Boolean = false,
    /** 待确认删除的探头卡片下标；非 null 时弹出确认框 */
    val deleteConfirmProbeIndex: Int? = null,
    val showSaveSuccessDialog: Boolean = false,
    val statusHint: String? = null,
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = ProbeConfigRepository(application)
    private val hostSettingsRepository = HostSettingsRepository(application)
    private val doseHistoryRepository = ProbeDoseHistoryRepository(application)
    private val alertLogRepository = AlertLogRepository(application)
    private val displaySoundController = DisplaySoundController(application)
    private val hostAlarmController = HostAlarmController(application)
    private val linkRouter = ProbeLinkRouter()
    private val hostEnvSerialRepository = HostEnvSerialRepository(
        onProbeFrame = ::onProbeSerialFrame,
    )
    private val zjbOtaClient = ZjbOtaClient(hostEnvSerialRepository)
    private val _zjbHardwareVersion = MutableStateFlow<String?>(null)
    val zjbHardwareVersion: StateFlow<String?> = _zjbHardwareVersion.asStateFlow()
    private var windowBrightnessApplier: ((Float) -> Unit)? = null
    private var pauseAlarmExpiryJob: Job? = null
    private val upperThresholdDebounceJobs = ConcurrentHashMap<Int, Job>()
    private val lowerThresholdDebounceJobs = ConcurrentHashMap<Int, Job>()
    /** 最近一次已成功写入设备的阈值（×100），避免重复下发 */
    private val lastCommittedUpperX100 = ConcurrentHashMap<String, Long>()
    private val lastCommittedLowerX100 = ConcurrentHashMap<String, Long>()
    private val discoveredMap = ConcurrentHashMap<String, DiscoveredDevice>()
    // private val multicastLogSeq = AtomicLong(0L) // 组播每秒日志已注释

    private val sensorOfflineLogAggregator = ProbeSensorOfflineLogAggregator(
        onSensorsNormal = { ts, probeName ->
            appendAlertLog(
                message = "$probeName 传感器正常",
                kind = AlertLogKind.Info,
                timestampMillis = ts,
            )
        },
        onSensorsOffline = { ts, probeName, sensors ->
            val labels = sensors.sortedBy { it.ordinal }.joinToString("、") { it.label }
            appendAlertLog(
                message = "$probeName ${labels}传感器离线",
                kind = AlertLogKind.Warning,
                timestampMillis = ts,
            )
        },
    )

    private val doseAlarmLogAggregator = ProbeDoseAlarmLogAggregator(
        onAlarmStarted = { ts, probeName, labels ->
            val ordered = listOf("上限", "下限").filter { it in labels }
            appendAlertLog(
                message = "$probeName 辐射${ordered.joinToString("、")}报警",
                kind = AlertLogKind.Alarm,
                timestampMillis = ts,
            )
        },
        onAlarmCleared = { ts, probeName ->
            appendAlertLog(
                message = "$probeName 辐射报警解除",
                kind = AlertLogKind.Info,
                timestampMillis = ts,
            )
        },
    )

    private val _savedProbes = MutableStateFlow(repository.load())
    private val _liveTelemetry = MutableStateFlow<Map<String, LiveProbeTelemetry>>(emptyMap())
    private val _nowMillis = MutableStateFlow(System.currentTimeMillis())
    private val _timeDisplaySettings = MutableStateFlow(hostSettingsRepository.loadTimeSettings())
    private val _uiFlags = MutableStateFlow(HomeUiFlags())
    /** 首页上次停留的探头（离开设置/侧栏再回来时恢复，不持久化到磁盘） */
    private val _homeSelectedProbeId = MutableStateFlow<String?>(null)
    private val _settings = MutableStateFlow(ProbeSettingsUiState())
    private val _hostNetwork = MutableStateFlow(hostSettingsRepository.loadHostNetwork())
    private val _hostConnectivity = MutableStateFlow(detectHostConnectivity(application))
    private val _alertLogs = MutableStateFlow(alertLogRepository.load())
    private var nextAlertLogId = (_alertLogs.value.maxOfOrNull { it.id } ?: 0L) + 1L
    private val _displaySoundSettings = MutableStateFlow(hostSettingsRepository.loadDisplaySound())

    private val connectionManager = ProbeConnectionManager(
        context = application,
        linkRouter = linkRouter,
        serialSender = { frame -> hostEnvSerialRepository.sendRaw(frame) },
        isTelemetryOnline = { probeId -> _liveTelemetry.value[probeId]?.isOnline == true },
        onDiscoveredRaw = ::onDiscoveryDatagram,
        onTcpFrame = ::onTcpFrame,
        onProbeOnlineChanged = ::onProbeOnlineChanged,
        onLog = { msg -> Log.i(ProbeConnectionManager.TAG, msg) },
    )

    /** 设置 · 时间页只读日期/时间，与主页共用每秒 tick */
    val systemTimeHint: StateFlow<String> = _nowMillis
        .map(::formatSystemTimeHint)
        .stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5_000),
            formatSystemTimeHint(_nowMillis.value),
        )

    val homeUiState: StateFlow<HomeUiState> = combine(
        combine(
            _savedProbes,
            _liveTelemetry,
            _nowMillis,
            _timeDisplaySettings,
        ) { saved, live, nowMillis, timeDisplay ->
            HomeClockInputs(saved, live, nowMillis, timeDisplay)
        },
        combine(
            _uiFlags,
            _alertLogs,
            _hostNetwork,
            hostEnvSerialRepository.snapshot,
            _hostConnectivity,
        ) { flags, logs, hostNetwork, hostAdapter, connectivity ->
            HomePanelInputs(flags, logs, hostNetwork, hostAdapter, connectivity)
        },
        _homeSelectedProbeId,
    ) { clockInputs, panel, homeSelectedProbeId ->
        buildHomeState(
            clockInputs.saved,
            clockInputs.live,
            clockInputs.nowMillis,
            clockInputs.timeDisplay,
            panel.flags,
            panel.logs,
            panel.hostNetwork,
            panel.hostAdapter,
            panel.connectivity,
            homeSelectedProbeId,
        )
    }.stateIn(
        viewModelScope,
        SharingStarted.WhileSubscribed(5_000),
        buildHomeState(
            _savedProbes.value,
            _liveTelemetry.value,
            _nowMillis.value,
            _timeDisplaySettings.value,
            _uiFlags.value,
            _alertLogs.value,
            _hostNetwork.value,
            hostEnvSerialRepository.snapshot.value,
            _hostConnectivity.value,
            _homeSelectedProbeId.value,
        ),
    )

    val settingsUiState: StateFlow<ProbeSettingsUiState> = _settings.asStateFlow()

    val displaySoundSettings: StateFlow<DisplaySoundSettings> = _displaySoundSettings.asStateFlow()

    private val _showSettings = MutableStateFlow(false)
    val settingsVisible: StateFlow<Boolean> = _showSettings.asStateFlow()

    init {
        connectionManager.setSavedProbes(_savedProbes.value)
        connectionManager.startDiscovery()
        val saved = _savedProbes.value
        Log.i(ProbeConnectionManager.TAG, "已加载 ${saved.size} 个已保存探头")
        if (saved.isNotEmpty()) {
            connectionManager.setSavedProbes(saved)
        }
        viewModelScope.launch {
            while (true) {
                _nowMillis.value = System.currentTimeMillis()
                pruneStaleDiscovery()
                pruneStaleProbeTelemetry()
                _hostConnectivity.value = detectHostConnectivity(getApplication())
                delay(1000L)
            }
        }
        schedulePauseAlarmExpiry(_displaySoundSettings.value.pauseAlarmUntilMillis)
        hostEnvSerialRepository.start()
        viewModelScope.launch {
            delay(PROBE_AUTO_SYNC_STARTUP_DELAY_MS)
            maybeAutoSyncTimeToOnlineProbes(reason = "startup")
        }
        viewModelScope.launch {
            combine(
                homeUiState.map { state -> state.slaveProbes.any { it.isOnline && it.hasAlarm } },
                _displaySoundSettings,
                _nowMillis,
            ) { alarming, sound, now ->
                Triple(alarming, sound.withExpiredPauseCleared(now), now)
            }.collect { (alarming, sound, now) ->
                hostAlarmController.sync(
                    shouldPlay = alarming,
                    volumeFraction = sound.hostAlarmVolume,
                    alarmSuppressed = sound.isHostAlarmSuppressed(now),
                )
            }
        }
    }

    fun openSettings() {
        val probes = _savedProbes.value
        val live = _liveTelemetry.value
        val loadedTime = hostSettingsRepository.loadTimeSettings()
        _settings.value = ProbeSettingsUiState(
            selectedTab = SettingsTab.DisplaySound,
            selectedProbeIndex = 0,
            // 左上角状态栏用 draft；须带上当前遥测，否则默认离线显示 ---
            manageDrafts = probes.map { probe ->
                probe.toManageDraft().mergeFromTelemetry(live[probe.id])
            },
            draftProbes = probes,
            discoveredDevices = discoveredMap.values.sortedForAddProbeDialog(),
            displaySound = loadDisplaySoundForUi(),
            hostNetwork = mergeLiveHostIp(_hostNetwork.value),
            slaveNetworkCards = hostSettingsRepository.buildSlaveNetworkCards(probes),
            timeSettings = loadedTime,
        )
        _showSettings.value = true
    }

    fun selectSettingsTab(tab: SettingsTab) {
        _settings.update { it.copy(selectedTab = tab, statusHint = null) }
        when (tab) {
            SettingsTab.DisplaySound -> syncDisplaySoundLevelsFromSystem()
            SettingsTab.Probes -> syncProbeCardAt(_settings.value.selectedProbeIndex)
            SettingsTab.Network -> refreshNetworkSettingsPanel()
            SettingsTab.About -> refreshZjbHardwareVersion()
            else -> Unit
        }
    }

    fun refreshZjbHardwareVersion() {
        viewModelScope.launch(Dispatchers.IO) {
            val version = runCatching { zjbOtaClient.readFirmwareVersion() }.getOrNull()
            _zjbHardwareVersion.value = version?.trim()?.takeIf { it.isNotEmpty() }
        }
    }

    suspend fun upgradeZjbFirmware(
        fileBytes: ByteArray,
        onProgress: (ZjbOtaProgress) -> Unit,
    ): Result<Unit> = withContext(Dispatchers.IO) {
        zjbOtaClient.upgrade(fileBytes, onProgress)
    }

    fun updateDisplaySound(settings: DisplaySoundSettings) {
        _displaySoundSettings.value = settings
        _settings.update { it.copy(displaySound = settings, statusHint = null) }
    }

    /** 静音：立即保存并停止本机报警音。 */
    fun commitDisplaySoundMute(mute: Boolean) {
        applyDisplaySound(_displaySoundSettings.value.copy(mute = mute))
    }

    /** 暂停报警 5 分钟：每次点击重新计时 5 分钟。 */
    fun triggerDisplaySoundPauseAlarm() {
        val until = System.currentTimeMillis() + PAUSE_ALARM_DURATION_MS
        applyDisplaySound(_displaySoundSettings.value.copy(pauseAlarmUntilMillis = until))
        schedulePauseAlarmExpiry(until)
    }

    private fun applyDisplaySound(settings: DisplaySoundSettings) {
        val normalized = settings.withExpiredPauseCleared()
        _displaySoundSettings.value = normalized
        _settings.update { it.copy(displaySound = normalized, statusHint = null) }
        hostSettingsRepository.saveDisplaySound(normalized)
    }

    private fun schedulePauseAlarmExpiry(untilMillis: Long) {
        pauseAlarmExpiryJob?.cancel()
        if (untilMillis <= System.currentTimeMillis()) return
        val delayMs = untilMillis - System.currentTimeMillis()
        pauseAlarmExpiryJob = viewModelScope.launch {
            delay(delayMs)
            val current = _displaySoundSettings.value
            if (current.pauseAlarmUntilMillis > 0L) {
                applyDisplaySound(current.copy(pauseAlarmUntilMillis = 0L))
            }
        }
    }

    fun bindWindowBrightnessApplier(applier: ((Float) -> Unit)?) {
        windowBrightnessApplier = applier
    }

    fun applyInitialWindowBrightness(brightness: Float) {
        windowBrightnessApplier?.invoke(brightness.coerceIn(0f, 1f))
    }

    /** 拖动亮度滑条时实时预览。 */
    fun previewDisplaySoundBrightness(brightness: Float) {
        windowBrightnessApplier?.invoke(brightness.coerceIn(0f, 1f))
    }

    private fun applyBrightnessToDevice(brightness: Float): Boolean {
        val clamped = brightness.coerceIn(0f, 1f)
        windowBrightnessApplier?.invoke(clamped)
        return displaySoundController.applySystemBrightness(clamped)
    }

    /** 松手：写入亮度并保存到本机偏好。 */
    fun commitDisplaySoundBrightness() {
        val displaySound = _displaySoundSettings.value
        val systemOk = applyBrightnessToDevice(displaySound.brightness)
        persistDisplaySoundSettings()
        _settings.update {
            it.copy(
                statusHint = if (systemOk) {
                    null
                } else {
                    "已调节当前应用亮度；全局系统亮度需在系统设置中授予「修改系统设置」权限"
                },
            )
        }
    }

    /** 松手：写入系统音量并保存到本机偏好。 */
    fun commitDisplaySoundSystemVolume() {
        val displaySound = _displaySoundSettings.value
        val ok = displaySoundController.applySystemVolume(displaySound.systemVolume)
        persistDisplaySoundSettings()
        if (!ok) {
            _settings.update { it.copy(statusHint = "系统音量调节失败") }
        } else {
            _settings.update { it.copy(statusHint = null) }
            if (!displaySound.mute) {
                displaySoundController.playSystemVolumePreview()
            }
        }
    }

    /** 松手：保存本机报警音量并播放报警预览音。 */
    fun commitDisplaySoundHostAlarmVolume() {
        val displaySound = _displaySoundSettings.value
        applyDisplaySound(displaySound)
        _settings.update { it.copy(statusHint = null) }
        if (!displaySound.isHostAlarmSuppressed()) {
            displaySoundController.playHostAlarmPreview(displaySound.hostAlarmVolume)
        }
    }

    /** 松手：保存提示音量并播放提示预览音。 */
    fun commitDisplaySoundPromptVolume() {
        val displaySound = _displaySoundSettings.value
        applyDisplaySound(displaySound)
        _settings.update { it.copy(statusHint = null) }
        if (!displaySound.mute) {
            displaySoundController.playPromptPreview(displaySound.promptVolume)
        }
    }

    fun saveDisplaySoundSettings() {
        val displaySound = _displaySoundSettings.value
        applyBrightnessToDevice(displaySound.brightness)
        displaySoundController.applySystemVolume(displaySound.systemVolume)
        persistDisplaySoundSettings()
        showSettingsSaveSuccess()
        _settings.update { it.copy(statusHint = null) }
        Log.i(ProbeConnectionManager.TAG, "显示与声音本机设置已保存")
    }

    private fun persistDisplaySoundSettings() {
        hostSettingsRepository.saveDisplaySound(_displaySoundSettings.value)
    }

    private fun loadDisplaySoundForUi(): DisplaySoundSettings {
        val saved = _displaySoundSettings.value.withExpiredPauseCleared()
        val systemVolume = displaySoundController.readSystemLevels().systemVolume
        return saved.copy(systemVolume = systemVolume)
    }

    private fun syncDisplaySoundLevelsFromSystem() {
        val systemVolume = displaySoundController.readSystemLevels().systemVolume
        _settings.update { state ->
            state.copy(
                displaySound = state.displaySound.copy(systemVolume = systemVolume),
                statusHint = null,
            )
        }
    }

    fun updateHostNetwork(settings: HostNetworkSettings) {
        val merged = mergeLiveHostIp(settings)
        _hostNetwork.value = merged
        _settings.update { it.copy(hostNetwork = merged, statusHint = null) }
    }

    fun updateSlaveNetworkCard(index: Int, card: SlaveNetworkCard) {
        _settings.update { state ->
            if (index !in state.slaveNetworkCards.indices) return@update state
            val cards = state.slaveNetworkCards.toMutableList()
            cards[index] = card
            state.copy(slaveNetworkCards = cards, statusHint = null)
        }
    }

    /**
     * 从主机侧 HLK-7688（同网段 `.1`）拉取 WiFi 名称/密码。
     * 仅回调结果，不改持久化；由编辑框回填后用户点「保存」。
     */
    fun fetchHostWifiFromGateway(
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) {
        viewModelScope.launch {
            _settings.update { it.copy(statusHint = "正在从主机网关获取 WiFi…") }
            val hostIp = mergeLiveHostIp(_hostNetwork.value).ipAddress
            if (hostIp.isBlank()) {
                val msg = "主机 IP 未知，无法推导网关"
                _settings.update { it.copy(statusHint = msg) }
                onDone(false, msg, null, null)
                return@launch
            }
            val result = withContext(Dispatchers.IO) {
                Hlk7688WifiClient.fetchHostWifi(hostIp)
            }
            deliverWifiFetchResult(result, role = "主机", onDone)
        }
    }

    /**
     * 从从机侧 HLK-7688 拉取 WiFi：设备 ID = n → 同网段 `.(n+1)`。
     * 网段优先用主机 IP，其次用该从机探头 IP。
     */
    fun fetchSlaveWifiFromGateway(
        deviceId: Int,
        slaveIp: String = "",
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) {
        viewModelScope.launch {
            _settings.update { it.copy(statusHint = "正在从从机网关获取 WiFi…") }
            val subnetIp = mergeLiveHostIp(_hostNetwork.value).ipAddress
                .takeIf { it.isNotBlank() }
                ?: slaveIp.trim().takeIf { it.isNotBlank() }
            if (subnetIp.isNullOrBlank()) {
                val msg = "主机/从机 IP 未知，无法推导从机网关"
                _settings.update { it.copy(statusHint = msg) }
                onDone(false, msg, null, null)
                return@launch
            }
            val result = withContext(Dispatchers.IO) {
                Hlk7688WifiClient.fetchSlaveWifi(subnetIp, deviceId)
            }
            deliverWifiFetchResult(result, role = "从机ID=$deviceId", onDone)
        }
    }

    private fun deliverWifiFetchResult(
        result: Hlk7688WifiClient.FetchResult,
        role: String,
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) {
        when (result) {
            is Hlk7688WifiClient.FetchResult.Ok -> {
                val cred = result.credentials
                val msg = "已从 ${cred.gatewayIp} 获取 WiFi，请确认后保存"
                _settings.update { it.copy(statusHint = msg) }
                Log.i(
                    ProbeConnectionManager.TAG,
                    "$role WiFi 已从 ${cred.gatewayIp} 获取 ssid=${cred.ssid}",
                )
                onDone(true, msg, cred.ssid, cred.password)
            }
            is Hlk7688WifiClient.FetchResult.Err -> {
                _settings.update { it.copy(statusHint = result.message) }
                onDone(false, result.message, null, null)
            }
        }
    }

    fun commitHostNetwork(settings: HostNetworkSettings) {
        val merged = mergeLiveHostIp(settings)
        _hostNetwork.value = merged
        _settings.update { it.copy(hostNetwork = merged, statusHint = null) }
        hostSettingsRepository.saveHostNetwork(merged)
        showSettingsSaveSuccess()
        Log.i(ProbeConnectionManager.TAG, "主机网络信息已保存")
    }

    fun commitSlaveNetwork(index: Int, card: SlaveNetworkCard) {
        updateSlaveNetworkCard(index, card)
        hostSettingsRepository.saveSlaveNetworkCards(_settings.value.slaveNetworkCards)
        showSettingsSaveSuccess()
        Log.i(
            ProbeConnectionManager.TAG,
            "从机网络信息已保存 probe=${card.probeId}",
        )
    }

    fun saveHostNetworkSection() {
        commitHostNetwork(_settings.value.hostNetwork)
    }

    fun saveSlaveNetworkSection(index: Int) {
        val state = _settings.value
        if (index !in state.slaveNetworkCards.indices) return
        commitSlaveNetwork(index, state.slaveNetworkCards[index])
    }

    fun updateTimeSettings(settings: TimeSettings) {
        _settings.update { it.copy(timeSettings = settings, statusHint = null) }
        hostSettingsRepository.saveTimeSettings(settings)
        _timeDisplaySettings.value = settings
    }

    fun syncTimeToDevice() {
        val result = pushTimeSyncToProbes(manual = true)
        _settings.update { it.copy(statusHint = result.userHint) }
    }

    private fun maybeAutoSyncTimeToOnlineProbes(reason: String) {
        if (!_timeDisplaySettings.value.autoSyncToProbe) return
        val result = pushTimeSyncToProbes(manual = false)
        if (result.syncedCount > 0) {
            Log.i(
                ProbeConnectionManager.TAG,
                "自动时间同步($reason) 已向 ${result.syncedCount} 个探头写入 reg94",
            )
        }
    }

    private fun maybeAutoSyncTimeToProbe(probeId: String, reason: String) {
        if (!_timeDisplaySettings.value.autoSyncToProbe) return
        val probe = _savedProbes.value.find { it.id == probeId } ?: return
        if (_liveTelemetry.value[probeId]?.isOnline != true) return
        val result = pushTimeSyncToProbes(manual = false, probeIds = setOf(probeId))
        if (result.syncedCount > 0) {
            Log.i(
                ProbeConnectionManager.TAG,
                "自动时间同步($reason) 已向 ${probe.displayName} 写入 reg94",
            )
        }
    }

    private data class ProbeTimeSyncPushResult(
        val syncedCount: Int,
        val userHint: String?,
    )

    private fun pushTimeSyncToProbes(
        manual: Boolean,
        probeIds: Set<String>? = null,
    ): ProbeTimeSyncPushResult {
        val now = System.currentTimeMillis()
        if (!isHostTimeValidForProbeSync(now)) {
            val hint = hostTimeInvalidForProbeSyncHint()
            if (manual) {
                Log.w(ProbeConnectionManager.TAG, "手动时间同步跳过：$hint")
                return ProbeTimeSyncPushResult(0, hint)
            }
            Log.w(ProbeConnectionManager.TAG, "自动时间同步跳过：$hint")
            return ProbeTimeSyncPushResult(0, null)
        }
        val probes = _savedProbes.value
        if (probes.isEmpty()) {
            return ProbeTimeSyncPushResult(
                0,
                if (manual) "无已保存探头，无法同步时间" else null,
            )
        }
        val live = _liveTelemetry.value
        val online = probes.filter { live[it.id]?.isOnline == true }
        val targets = if (probeIds == null) {
            online
        } else {
            online.filter { it.id in probeIds }
        }
        if (targets.isEmpty()) {
            return ProbeTimeSyncPushResult(
                0,
                if (manual) "无在线探头，无法同步时间" else null,
            )
        }
        val syncedNames = mutableListOf<String>()
        var skippedNoRoute = 0
        for (probe in targets) {
            if (!manual) {
                val syncState = hostSettingsRepository.loadProbeTimeSyncState(probe.id)
                if (!shouldAutoSyncProbeTime(
                        lastAutoSyncMillis = syncState.lastAutoSyncMillis,
                        lastOfflineMillis = syncState.lastOfflineMillis,
                        nowMillis = now,
                    )
                ) {
                    continue
                }
            }
            if (linkRouter.routeFor(probe.id) == null) {
                skippedNoRoute++
                if (manual) {
                    Log.i(
                        ProbeConnectionManager.TAG,
                        "跳过时间同步 ${probe.displayName}：尚无 0x23 路由",
                    )
                }
                continue
            }
            connectionManager.sendFrames(probe.id, listOf(probe.buildTimeSyncWriteFrame()))
            syncedNames += probe.displayName
            if (!manual) {
                val prev = hostSettingsRepository.loadProbeTimeSyncState(probe.id)
                hostSettingsRepository.saveProbeTimeSyncState(
                    probe.id,
                    prev.copy(lastAutoSyncMillis = now),
                )
            }
            Log.i(
                ProbeConnectionManager.TAG,
                "${if (manual) "手动" else "自动"}时间同步 ${probe.displayName} " +
                    "reg94 addr=0x${probe.modbusDeviceAddr().toUByte().toString(16)}",
            )
        }
        val sent = syncedNames.size
        val hint = if (!manual) {
            null
        } else when {
            sent == 0 && skippedNoRoute > 0 -> "时间同步失败：在线探头尚无通信路由"
            sent == 0 -> "无符合条件的在线探头可同步时间"
            skippedNoRoute > 0 -> "已向 $sent 个探头同步本机时间（$skippedNoRoute 个无路由已跳过）"
            sent == 1 -> "已向 ${syncedNames.first()} 同步本机时间"
            else -> "已向 $sent 个在线探头同步本机时间"
        }
        return ProbeTimeSyncPushResult(sent, hint)
    }

    private fun recordProbeOfflineForTimeSync(probeId: String) {
        val now = System.currentTimeMillis()
        val prev = hostSettingsRepository.loadProbeTimeSyncState(probeId)
        hostSettingsRepository.saveProbeTimeSyncState(
            probeId,
            prev.copy(lastOfflineMillis = now),
        )
    }

    fun aboutDeviceInfo(): AboutDeviceInfo {
        val app = getApplication<Application>()
        val version = run {
            @Suppress("DEPRECATION")
            val pkg = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                app.packageManager.getPackageInfo(
                    app.packageName,
                    PackageManager.PackageInfoFlags.of(0),
                )
            } else {
                app.packageManager.getPackageInfo(app.packageName, 0)
            }
            pkg.versionName
        } ?: "—"
        val formFactor = ScreenSpec.formFactor(
            app.resources.configuration.screenWidthDp,
            app.resources.configuration.screenHeightDp,
        )
        val model = when (formFactor) {
            TabletFormFactor.Compact -> "NS-T100（10 寸）"
            TabletFormFactor.Expanded -> "NS-T130（13 寸）"
        }
        val serial = run {
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    Build.getSerial()
                } else {
                    @Suppress("DEPRECATION")
                    Build.SERIAL
                }
            } catch (_: SecurityException) {
                "—"
            }
        }.takeIf { it.isNotBlank() && it != Build.UNKNOWN } ?: "—"
        return AboutDeviceInfo(
            productName = "NetShield 联盾环境辐射监测系统",
            hostModel = model,
            serialNumber = serial,
            softwareVersion = version,
            hardwareVersion = _zjbHardwareVersion.value?.takeIf { it.isNotBlank() } ?: "—",
        )
    }

    fun systemTimeDisplayText(): String = formatSystemTimeHint(_nowMillis.value)

    private fun formatSystemTimeHint(millis: Long): String {
        val fmt = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        return fmt.format(Date(millis))
    }

    private fun showSettingsSaveSuccess() {
        _settings.update { it.copy(showSaveSuccessDialog = true, statusHint = null) }
    }

    private fun refreshNetworkSettingsPanel() {
        val merged = mergeLiveHostIp(_hostNetwork.value)
        _hostNetwork.value = merged
        _settings.update { state ->
            state.copy(
                hostNetwork = merged,
                slaveNetworkCards = hostSettingsRepository.buildSlaveNetworkCards(state.draftProbes),
            )
        }
    }

    private fun mergeLiveHostIp(host: HostNetworkSettings): HostNetworkSettings {
        val options = listFsyNetworkOptions(getApplication())
        val liveIp = options.firstOrNull { it.isDefault }?.localIpv4?.takeIf { it.isNotBlank() }
            ?: options.firstOrNull()?.localIpv4
        return if (!liveIp.isNullOrBlank()) host.copy(ipAddress = liveIp) else host
    }

    fun selectProbePage(index: Int) {
        val clamped = index.coerceIn(0, _settings.value.manageDrafts.lastIndex.coerceAtLeast(0))
        _settings.update { it.copy(selectedProbeIndex = clamped) }
        syncProbeCardAt(clamped)
    }

    /** 首页 Pager 停留页对应的探头，离开再回来时恢复到该探头所在页 */
    fun selectHomeProbe(probeId: String) {
        if (probeId.isBlank() || probeId == "placeholder") return
        if (_homeSelectedProbeId.value == probeId) return
        _homeSelectedProbeId.value = probeId
    }

    /** 进入设置页或切换探头卡片时：从遥测合并一次并主动读 reg50/52/82/122/123；停留期间 0x23 不刷新阈值/使能表单 */
    private fun syncProbeCardAt(index: Int) {
        val state = _settings.value
        if (index !in state.manageDrafts.indices) return
        val probe = state.draftProbes.getOrNull(index) ?: return
        val telemetry = _liveTelemetry.value[probe.id]
        _settings.update { s ->
            if (index !in s.manageDrafts.indices) return@update s
            val drafts = s.manageDrafts.toMutableList()
            val merged = drafts[index]
                .mergeConfigFromTelemetry(telemetry)
                .copy(isTcpOnline = telemetry?.isOnline == true)
            drafts[index] = merged
            seedLastCommittedThresholds(probe.id, merged)
            s.copy(manageDrafts = drafts)
        }
        if (telemetry?.isOnline == true) {
            viewModelScope.launch {
                // 先让探头管理页完成首帧绘制，再发 0x03，减轻切 Tab 时「卡住」感
                delay(80)
                val current = _settings.value
                if (index !in current.manageDrafts.indices) return@launch
                if (current.selectedProbeIndex != index) return@launch
                val p = current.draftProbes.getOrNull(index) ?: return@launch
                if (_liveTelemetry.value[p.id]?.isOnline != true) return@launch
                withContext(Dispatchers.IO) {
                    connectionManager.fetchManageConfigWithRetry(p)
                }
                if (_settings.value.selectedProbeIndex == index &&
                    _settings.value.manageDrafts.getOrNull(index)?.id == p.id
                ) {
                    applyConfigReadToSelectedProbeDraft(p.id)
                }
            }
        } else {
            Log.i(ProbeConnectionManager.TAG, "跳过读配置 ${probe.displayName}：探头离线")
        }
    }

    /** 音量滑条松手后写入 reg 122 */
    fun commitProbeVolume(index: Int) {
        val draft = _settings.value.manageDrafts.getOrNull(index) ?: return
        if (!draft.isTcpOnline) return
        viewModelScope.launch {
            val result = connectionManager.sendFramesWithRetry(
                draft.savedProbe,
                listOf(draft.buildVolumeWriteFrame()),
                "写音量",
            )
            if (!result.complete) {
                Log.w(ProbeConnectionManager.TAG, "音量写入失败 probe=${draft.id} reg122")
                return@launch
            }
            _liveTelemetry.update { map ->
                val t = map[draft.id] ?: LiveProbeTelemetry()
                map + (draft.id to t.copy(volume = draft.volume))
            }
            Log.d(ProbeConnectionManager.TAG, "音量已写入 probe=${draft.id} reg122")
        }
    }

    fun updateManageDraft(index: Int, draft: ProbeManageDraft) {
        val prev = _settings.value.manageDrafts.getOrNull(index)
        _settings.update { state ->
            if (index !in state.manageDrafts.indices) return@update state
            val drafts = state.manageDrafts.toMutableList()
            drafts[index] = draft
            state.copy(
                manageDrafts = drafts,
                draftProbes = drafts.map { it.savedProbe },
                statusHint = null,
            )
        }
        if (prev != null) {
            if (prev.doseUpperUsv != draft.doseUpperUsv) {
                scheduleUpperThresholdWriteDebounced(index)
            }
            if (prev.doseLowerUsv != draft.doseLowerUsv) {
                scheduleLowerThresholdWriteDebounced(index)
            }
            pushDraftWritesIfNeeded(prev, draft)
        }
    }

    /**
     * 即时写从机：
     * - 阈值输入停止 [THRESHOLD_DEBOUNCE_MS] 后 → reg 50 / reg 52
     * - 上限/下限旁「报警」→ reg 82
     * - 从机屏幕 / 报警灯光 → reg 123 bit14 / bit13
     * - 音量滑条松手 → reg 122
     */
    private fun pushDraftWritesIfNeeded(prev: ProbeManageDraft, draft: ProbeManageDraft) {
        if (!draft.isTcpOnline) return
        val probeId = draft.id
        val frames = mutableListOf<ByteArray>()
        val alarmChanged = prev.radiationUpperAlarmOn != draft.radiationUpperAlarmOn ||
            prev.radiationLowerAlarmOn != draft.radiationLowerAlarmOn
        val controlChanged = prev.slaveScreenOn != draft.slaveScreenOn ||
            prev.alarmLightOn != draft.alarmLightOn

        if (alarmChanged) {
            frames += draft.buildAlarmEnableWriteFrame()
        }
        if (controlChanged) {
            frames += draft.buildControlBit2WriteFrame()
        }
        if (frames.isEmpty()) return

        viewModelScope.launch {
            val result = connectionManager.sendFramesWithRetry(
                draft.savedProbe,
                frames,
                "写探头配置",
            )
            if (!result.complete) {
                Log.w(ProbeConnectionManager.TAG, "探头配置写入失败 probe=$probeId frames=${frames.size}")
                return@launch
            }
            if (alarmChanged) {
                patchTelemetryAlarmEnable(probeId, draft)
            }
            if (controlChanged) {
                patchTelemetryControlBit2(probeId, draft)
            }
            val detail = buildList {
                if (prev.radiationUpperAlarmOn != draft.radiationUpperAlarmOn) {
                    add("上报警=${draft.radiationUpperAlarmOn}")
                }
                if (prev.radiationLowerAlarmOn != draft.radiationLowerAlarmOn) {
                    add("下报警=${draft.radiationLowerAlarmOn}")
                }
                if (prev.slaveScreenOn != draft.slaveScreenOn) {
                    add("背光=${draft.slaveScreenOn}")
                }
                if (prev.alarmLightOn != draft.alarmLightOn) {
                    add("灯光=${draft.alarmLightOn}")
                }
            }.joinToString(", ")
            Log.d(
                ProbeConnectionManager.TAG,
                "探头配置写入 probe=$probeId frames=${frames.size} ($detail)",
            )
        }
    }

    private fun patchTelemetryControlBit2(probeId: String, draft: ProbeManageDraft) {
        val merged = mergeControlBit2Enables(draft.controlBit2Raw, draft.slaveScreenOn, draft.alarmLightOn)
        _liveTelemetry.update { map ->
            val t = map[probeId] ?: LiveProbeTelemetry()
            map + (probeId to t.copy(
                controlBit2Value = merged,
                slaveScreenOn = draft.slaveScreenOn,
                alarmLightOn = draft.alarmLightOn,
            ))
        }
    }

    private fun patchTelemetryAlarmEnable(probeId: String, draft: ProbeManageDraft) {
        _liveTelemetry.update { map ->
            val t = map[probeId] ?: LiveProbeTelemetry()
            map + (probeId to t.copy(
                radiationUpperAlarmOn = draft.radiationUpperAlarmOn,
                radiationLowerAlarmOn = draft.radiationLowerAlarmOn,
            ))
        }
    }

    private fun scheduleUpperThresholdWriteDebounced(index: Int) {
        upperThresholdDebounceJobs[index]?.cancel()
        upperThresholdDebounceJobs[index] = viewModelScope.launch {
            delay(THRESHOLD_DEBOUNCE_MS)
            commitProbeUpperThreshold(index)
        }
    }

    private fun scheduleLowerThresholdWriteDebounced(index: Int) {
        lowerThresholdDebounceJobs[index]?.cancel()
        lowerThresholdDebounceJobs[index] = viewModelScope.launch {
            delay(THRESHOLD_DEBOUNCE_MS)
            commitProbeLowerThreshold(index)
        }
    }

    private suspend fun commitProbeUpperThreshold(index: Int) {
        val draft = _settings.value.manageDrafts.getOrNull(index) ?: return
        if (!draft.isTcpOnline) return
        val x100 = usvTextToX100(draft.doseUpperUsv) ?: return
        if (lastCommittedUpperX100[draft.id] == x100) return
        val frame = draft.buildDoseUpperWriteFrame() ?: return
        val result = connectionManager.sendFramesWithRetry(
            draft.savedProbe,
            listOf(frame),
            "写上限阈值",
        )
        if (!result.complete) {
            Log.w(
                ProbeConnectionManager.TAG,
                "上限阈值写入失败 probe=${draft.id} reg50=${draft.doseUpperUsv} μSv/h",
            )
            return
        }
        lastCommittedUpperX100[draft.id] = x100
        _liveTelemetry.update { map ->
            val t = map[draft.id] ?: LiveProbeTelemetry()
            map + (draft.id to t.copy(doseUpperUsv = draft.doseUpperUsv))
        }
        Log.d(
            ProbeConnectionManager.TAG,
            "上限阈值已写入 probe=${draft.id} reg50=${draft.doseUpperUsv} μSv/h",
        )
    }

    private suspend fun commitProbeLowerThreshold(index: Int) {
        val draft = _settings.value.manageDrafts.getOrNull(index) ?: return
        if (!draft.isTcpOnline) return
        val x100 = usvTextToX100(draft.doseLowerUsv) ?: return
        if (lastCommittedLowerX100[draft.id] == x100) return
        val frame = draft.buildDoseLowerWriteFrame() ?: return
        val result = connectionManager.sendFramesWithRetry(
            draft.savedProbe,
            listOf(frame),
            "写下限阈值",
        )
        if (!result.complete) {
            Log.w(
                ProbeConnectionManager.TAG,
                "下限阈值写入失败 probe=${draft.id} reg52=${draft.doseLowerUsv} μSv/h",
            )
            return
        }
        lastCommittedLowerX100[draft.id] = x100
        _liveTelemetry.update { map ->
            val t = map[draft.id] ?: LiveProbeTelemetry()
            map + (draft.id to t.copy(doseLowerUsv = draft.doseLowerUsv))
        }
        Log.d(
            ProbeConnectionManager.TAG,
            "下限阈值已写入 probe=${draft.id} reg52=${draft.doseLowerUsv} μSv/h",
        )
    }

    private fun seedLastCommittedThresholds(probeId: String, draft: ProbeManageDraft) {
        usvTextToX100(draft.doseUpperUsv)?.let { lastCommittedUpperX100[probeId] = it }
        usvTextToX100(draft.doseLowerUsv)?.let { lastCommittedLowerX100[probeId] = it }
    }

    private fun cancelThresholdDebounceJobs() {
        upperThresholdDebounceJobs.values.forEach { it.cancel() }
        lowerThresholdDebounceJobs.values.forEach { it.cancel() }
        upperThresholdDebounceJobs.clear()
        lowerThresholdDebounceJobs.clear()
    }

    fun closeSettings() {
        cancelThresholdDebounceJobs()
        _showSettings.value = false
        _settings.update { it.copy(showAddProbeDialog = false, showSaveSuccessDialog = false) }
        val savedBrightness = hostSettingsRepository.loadDisplaySound().brightness
        windowBrightnessApplier?.invoke(savedBrightness)
    }

    fun showAddProbeDialog() {
        connectionManager.restartDiscovery()
        _settings.update {
            it.copy(
                showAddProbeDialog = true,
                discoveredDevices = discoveredMap.values.sortedForAddProbeDialog(),
                statusHint = if (discoveredMap.isEmpty()) {
                    "正在搜索设备（组播 / 串口）…"
                } else {
                    null
                },
            )
        }
    }

    fun dismissAddProbeDialog() {
        _settings.update { it.copy(showAddProbeDialog = false) }
    }

    fun addProbeFromDiscovery(device: DiscoveredDevice) {
        val state = _settings.value
        if (state.draftProbes.any { matchesSaved(it, device) }) {
            _settings.update { it.copy(statusHint = "该设备已在列表中") }
            return
        }
        val added = device.toSavedProbe()
        val newDraft = added.toManageDraft()
        val drafts = state.manageDrafts + newDraft
        val list = drafts.map { it.savedProbe }
        persistProbeList(list)
        if (added.ip.isNotBlank()) {
            connectionManager.connectAfterDiscovery(added, device)
        }
        val newIndex = drafts.lastIndex
        _settings.update {
            it.copy(
                manageDrafts = drafts,
                draftProbes = list,
                slaveNetworkCards = hostSettingsRepository.buildSlaveNetworkCards(list),
                selectedProbeIndex = newIndex,
                showAddProbeDialog = false,
                showSaveSuccessDialog = true,
                statusHint = null,
            )
        }
        syncProbeCardAt(newIndex)
        Log.i(ProbeConnectionManager.TAG, "探头已添加并保存 id=${added.id} name=${added.displayName}")
    }

    fun requestRemoveProbe(index: Int) {
        _settings.update { state ->
            if (index !in state.manageDrafts.indices) return@update state
            if (state.manageDrafts[index].isTcpOnline) return@update state
            state.copy(deleteConfirmProbeIndex = index, statusHint = null)
        }
    }

    fun dismissRemoveProbeConfirm() {
        _settings.update { it.copy(deleteConfirmProbeIndex = null) }
    }

    fun confirmRemoveProbe() {
        val index = _settings.value.deleteConfirmProbeIndex ?: return
        _settings.update { it.copy(deleteConfirmProbeIndex = null) }
        removeProbePermanently(index)
    }

    private fun removeProbePermanently(index: Int) {
        val state = _settings.value
        if (index !in state.manageDrafts.indices) return
        val removed = state.manageDrafts[index]
        val removedId = removed.id
        val drafts = state.manageDrafts.filterIndexed { i, _ -> i != index }
        val list = drafts.map { it.savedProbe }
        val newIndex = state.selectedProbeIndex.coerceIn(0, (drafts.size - 1).coerceAtLeast(0))
        repository.save(list)
        _savedProbes.value = list
        connectionManager.setSavedProbes(list)
        connectionManager.disconnect(removedId)
        linkRouter.clear(removedId)
        _liveTelemetry.update { map -> map.filterKeys { id -> id != removedId } }
        _settings.update {
            it.copy(
                manageDrafts = drafts,
                draftProbes = list,
                selectedProbeIndex = newIndex,
                statusHint = if (list.isEmpty()) {
                    "已删除 ${removed.displayName}，列表为空"
                } else {
                    "已删除 ${removed.displayName}"
                },
            )
        }
        Log.i(ProbeConnectionManager.TAG, "探头已删除 id=$removedId name=${removed.displayName}")
        if (drafts.isNotEmpty()) {
            syncProbeCardAt(newIndex)
        }
    }

    /** 仅持久化到本机：探头列表、名称、位置；设备参数已在编辑时即时下发。 */
    fun saveProbeSettings() {
        val list = _settings.value.manageDrafts.map { it.savedProbe }
        persistProbeList(list)
        _settings.update {
            it.copy(
                draftProbes = list,
                showSaveSuccessDialog = true,
                statusHint = null,
            )
        }
        Log.i(ProbeConnectionManager.TAG, "探头列表已保存到本机 count=${list.size}")
    }

    /** 探头详情页回写名称/位置，统一落到 SavedProbe 与设置草稿。 */
    fun updateProbeIdentity(probeId: String, displayName: String, location: String) {
        val name = displayName.trim().ifBlank { "Detector" }
        val loc = location.trim()
        val current = _savedProbes.value
        if (current.none { it.id == probeId }) return

        val updated = current.map { probe ->
            if (probe.id == probeId) {
                probe.copy(displayName = name, location = loc)
            } else {
                probe
            }
        }
        persistProbeList(updated)

        _settings.update { state ->
            val drafts = state.manageDrafts.map { draft ->
                if (draft.id == probeId) {
                    draft.withDisplayName(name).withLocation(loc)
                } else {
                    draft
                }
            }
            state.copy(
                manageDrafts = drafts,
                draftProbes = updated,
                slaveNetworkCards = hostSettingsRepository.buildSlaveNetworkCards(updated),
            )
        }
    }

    private fun persistProbeList(list: List<SavedProbe>) {
        val prevIds = _savedProbes.value.map { it.id }.toSet()
        val newIds = list.map { it.id }.toSet()
        repository.save(list)
        _savedProbes.value = list
        connectionManager.setSavedProbes(list)
        if (prevIds != newIds) {
            connectionManager.reconnectAll(list)
            _liveTelemetry.value = _liveTelemetry.value.filterKeys { id -> id in newIds }
        }
    }

    fun dismissSaveSuccessDialog() {
        _settings.update { it.copy(showSaveSuccessDialog = false) }
    }

    fun toggleStatusBar() {
        _uiFlags.update { it.copy(statusBarExpanded = !it.statusBarExpanded) }
    }

    fun setStatusBarExpanded(expanded: Boolean) {
        _uiFlags.update { it.copy(statusBarExpanded = expanded) }
    }

    fun setSideDrawerOpen(open: Boolean) {
        _uiFlags.update { it.copy(sideDrawerOpen = open) }
    }

    private fun onDiscoveryDatagram(text: String) {
        val broadcast = parseFsyBroadcast(text) ?: run {
            Log.w(ProbeConnectionManager.TAG, "组播解析失败 raw=${text.take(120)}")
            return
        }
        val device = DiscoveredDevice.fromBroadcast(broadcast)
        // 每秒组播刷屏，调试时再打开
        // Log.i(
        //     ProbeConnectionManager.TAG,
        //     "组播收到 #${multicastLogSeq.incrementAndGet()} ${device.model} ${device.ip} " +
        //         "serial=${device.serial} id=${device.protoAddr}",
        // )
        discoveredMap[device.stableId] = device
        val matchedProbeId = _savedProbes.value
            .firstOrNull { matchesSaved(it, device) && it.ip.isNotBlank() }
            ?.id
        if (matchedProbeId != null) {
            linkRouter.recordMulticastKeepalive(matchedProbeId)
        }
        viewModelScope.launch(Dispatchers.Default) {
            matchedProbeId?.let { markNetworkProbeUiOnline(it) }
            applyDiscoveryNetworkUpdate(device)
            publishDiscoveredDevicesIfVisible()
        }
    }

    /** zjb 串口/CAN：非 0xEF 从机 0x23（序列号广播 + 实时上传） */
    private fun onProbeSerialFrame(frame: com.raydose.netshield.net.ParsedFsyFrame) {
        if (!frame.crcOk) return

        frame.deviceSerial?.trim()?.takeIf { it.isNotEmpty() }?.let { serial ->
            upsertSerialDiscovery(protoAddr = frame.addr, serial = serial)
            Log.i(ProbeConnectionManager.TAG, "串口发现探头 serial=$serial addr=0x${frame.addr.toString(16)}")
        }

        val isRealtimeUpload =
            frame.func == 0x23 && frame.uploadValues != null && frame.uploadValues.size >= 8
        val isFiveMinUpload = frame.func == 0x23 && frame.fiveMinUpload != null
        if (isRealtimeUpload || isFiveMinUpload) {
            upsertSerialDiscoveryFromRealtime(frame.addr)
            val probeId = findSavedProbeIdByModbusAddr(frame.addr) ?: return
            applyTelemetryFromFrame(probeId, frame, ProbeCommandLink.SERIAL)
            return
        }
        if (frame.func == 0x13 || frame.func == 0x16 || frame.func == 0x20) {
            val probeId = findSavedProbeIdByModbusAddr(frame.addr) ?: return
            applyTelemetryFromFrame(probeId, frame, null)
        }
    }

    /** 串口 SN 广播：写入发现表，并同步已保存探头（不覆盖已有 IP）。 */
    private fun upsertSerialDiscovery(protoAddr: Int, serial: String) {
        val device = DiscoveredDevice.fromSerialSerialUpload(protoAddr = protoAddr, serial = serial)
        discoveredMap[device.stableId] = device
        removeSerialDiscoveryPlaceholder(protoAddr)
        applyDiscoveryNetworkUpdate(device)
        publishDiscoveredDevicesIfVisible()
    }

    /** 仅有实时 0x23、尚未收到 SN 时，按协议地址占位，便于「添加探头」列表展示。 */
    private fun upsertSerialDiscoveryFromRealtime(protoAddr: Int) {
        val addrStr = protoAddr.toString()
        val now = System.currentTimeMillis()
        val bySerial = discoveredMap.values.firstOrNull {
            it.protoAddr == addrStr && it.serial.isNotBlank()
        }
        if (bySerial != null) {
            discoveredMap[bySerial.stableId] = bySerial.copy(lastSeenMillis = now)
            publishDiscoveredDevicesIfVisible()
            return
        }
        val placeholderKey = serialDiscoveryPlaceholderId(protoAddr)
        val existing = discoveredMap[placeholderKey]
        discoveredMap[placeholderKey] = (existing ?: DiscoveredDevice(
            model = "FSY-I",
            serial = "",
            ip = "",
            controlPort = 0,
            dataPort = 0,
            protoAddr = addrStr,
            lastSeenMillis = now,
        )).copy(lastSeenMillis = now)
        publishDiscoveredDevicesIfVisible()
    }

    private fun serialDiscoveryPlaceholderId(protoAddr: Int): String = "can_addr_$protoAddr"

    private fun removeSerialDiscoveryPlaceholder(protoAddr: Int) {
        discoveredMap.remove(serialDiscoveryPlaceholderId(protoAddr))
    }

    private fun publishDiscoveredDevicesIfVisible() {
        if (!_settings.value.showAddProbeDialog) return
        _settings.update {
            it.copy(
                discoveredDevices = discoveredMap.values.sortedForAddProbeDialog(),
                statusHint = null,
            )
        }
    }

    private fun findSavedProbeIdByModbusAddr(addr: Int): String? {
        return _savedProbes.value.firstOrNull { probe ->
            val n = probe.protoAddr.trim().toIntOrNull() ?: return@firstOrNull false
            n == addr
        }?.id
    }

    private fun applyTelemetryFromFrame(
        probeId: String,
        frame: com.raydose.netshield.net.ParsedFsyFrame,
        rxLink: ProbeCommandLink? = null,
    ) {
        if (!frame.crcOk) return

        if (rxLink != null &&
            frame.func == 0x23 &&
            frame.uploadValues != null &&
            frame.uploadValues.size >= 8 &&
            isPlausibleRealtimeDoseX100(frame.uploadValues[0])
        ) {
            linkRouter.recordRx23(probeId, rxLink)
        }
        if (rxLink != null && frame.func == 0x23 && frame.fiveMinUpload != null) {
            linkRouter.recordRx23(probeId, rxLink)
        }

        frame.fiveMinUpload?.let { fiveMin ->
            val deviceMillis = fiveMin.toEpochMillis()
            val kind = if (fiveMin.fromHistory) "hist" else "live"
            Log.i(
                ProbeConnectionManager.TAG,
                "5min RX($kind) probe=$probeId D5=${"%.6f".format(fiveMin.doseUsv)}uSv " +
                    "raw=${fiveMin.doseRaw} t=${fiveMin.timeString} deviceMs=$deviceMillis",
            )
            doseHistoryRepository.recordSampleIfDue(probeId, fiveMin.doseUsv, deviceMillis)
            if (!fiveMin.fromHistory) {
                maybeSyncProbeTimeOnFiveMinSkew(probeId, deviceMillis)
            }
        }

        if (frame.func == 0x13 || frame.func == 0x16 || frame.func == 0x20) {
            connectionManager.offerConfigReadFrame(probeId, frame)
        }

        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            val next = prev.applyParsedFrame(frame)
            map + (probeId to next)
        }
        if (_showSettings.value) {
            patchProbeRealtimeSummaryOnDraft(probeId)
        }
        if (frame.func == 0x13 && _showSettings.value) {
            applyConfigReadToSelectedProbeDraft(probeId)
        }
        frame.uploadValues
            ?.takeIf { it.size >= 8 && isPlausibleRealtimeDoseX100(it[0]) }
            ?.let {
            val probe = _savedProbes.value.find { it.id == probeId }
            val telemetry = _liveTelemetry.value[probeId]
            val probeName = probe?.displayName ?: probeId
            val alarmBit = telemetry?.alarmBit
            val now = System.currentTimeMillis()
            sensorOfflineLogAggregator.onTelemetrySample(
                probeId = probeId,
                probeName = probeName,
                alarmBit = alarmBit,
                nowMillis = now,
            )
            doseAlarmLogAggregator.onTelemetrySample(
                probeId = probeId,
                probeName = probeName,
                alarmBit = alarmBit,
                nowMillis = now,
            )
        }
    }

    /** 5min 帧设备时间与主机偏差过大时立即写 reg94（不受 6h 自动同步间隔限制）。 */
    private fun maybeSyncProbeTimeOnFiveMinSkew(probeId: String, deviceMillis: Long) {
        val now = System.currentTimeMillis()
        if (!isHostTimeValidForProbeSync(now)) return
        val skew = kotlin.math.abs(deviceMillis - now)
        if (skew <= PROBE_TIME_SYNC_ON_5MIN_SKEW_MS) return
        val probe = _savedProbes.value.find { it.id == probeId } ?: return
        if (linkRouter.routeFor(probeId) == null) return
        connectionManager.sendFrames(probeId, listOf(probe.buildTimeSyncWriteFrame()))
        Log.i(
            ProbeConnectionManager.TAG,
            "5min 时差同步 ${probe.displayName} skewMs=$skew > ${PROBE_TIME_SYNC_ON_5MIN_SKEW_MS}",
        )
    }

    /**
     * 按小时拆窗写 reg108/112，请求设备 Flash 历史；回传为 0x23 start=0x0024（不对时）。
     */
    fun requestFiveMinHistoryPull(probeId: String, hours: Int = 24) {
        if (!AUTO_FIVE_MIN_HISTORY_PULL) {
            Log.i(ProbeConnectionManager.TAG, "历史补拉已暂时关闭，跳过 probe=$probeId")
            return
        }
        val probe = _savedProbes.value.find { it.id == probeId } ?: return
        if (linkRouter.routeFor(probeId) == null) {
            Log.i(ProbeConnectionManager.TAG, "跳过历史补拉 ${probe.displayName}：无路由")
            return
        }
        val chunks = buildFiveMinHistoryHourChunks(hours.coerceIn(1, 24))
        viewModelScope.launch {
            Log.i(
                ProbeConnectionManager.TAG,
                "历史补拉开始 ${probe.displayName} hours=$hours chunks=${chunks.size}",
            )
            chunks.forEachIndexed { index, (start, end) ->
                connectionManager.sendFrames(
                    probeId,
                    listOf(probe.buildFiveMinHistoryRequestFrame(start, end)),
                )
                /* 给设备泵送该小时窗口留时间（最多约 12 条 × 50ms） */
                delay(1_500L)
                if (index < chunks.lastIndex) {
                    delay(200L)
                }
            }
            Log.i(ProbeConnectionManager.TAG, "历史补拉请求已发完 ${probe.displayName}")
        }
    }

    /**
     * 同一序列号设备 IP 变化（如上电 DHCP）时，同步已保存探头的网络信息并重连 TCP。
     */
    private fun applyDiscoveryNetworkUpdate(device: DiscoveredDevice) {
        val current = _savedProbes.value
        val probe = current.firstOrNull { matchesSaved(it, device) } ?: return
        val updated = probe.mergeFromDiscovery(device) ?: return

        val list = current.map { saved -> if (saved.id == probe.id) updated else saved }
        repository.save(list)
        _savedProbes.value = list
        connectionManager.setSavedProbes(list)

        if (probe.id != updated.id) {
            val telemetry = _liveTelemetry.value[probe.id] ?: LiveProbeTelemetry()
            linkRouter.routeFor(probe.id)?.let { linkRouter.recordRx23(updated.id, it) }
            linkRouter.clear(probe.id)
            _liveTelemetry.update { map ->
                map.filterKeys { it != probe.id } + (updated.id to telemetry)
            }
            connectionManager.disconnect(probe.id)
        } else if (probe.ip != updated.ip || probe.controlPort != updated.controlPort) {
            connectionManager.disconnect(probe.id)
        }

        _settings.update { state ->
            val drafts = state.manageDrafts.map { draft ->
                if (!matchesSaved(draft.savedProbe, device)) draft
                else draft.withSavedProbe(updated)
            }
            state.copy(
                manageDrafts = drafts,
                draftProbes = list,
                slaveNetworkCards = hostSettingsRepository.buildSlaveNetworkCards(list),
            )
        }

        Log.i(
            ProbeConnectionManager.TAG,
            "探头网络已同步 serial=${updated.serial} ${probe.ip} -> ${updated.ip} id=${updated.id}",
        )
    }

    private fun pruneStaleDiscovery() {
        val cutoff = System.currentTimeMillis() - DISCOVERY_TTL_MS
        val removed = discoveredMap.entries.removeIf { it.value.lastSeenMillis < cutoff }
        if (removed && _settings.value.showAddProbeDialog) {
            _settings.update {
                it.copy(discoveredDevices = discoveredMap.values.sortedForAddProbeDialog())
            }
        }
    }

    /**
     * 串口/CAN/LoRa：8s 无实时 0x23 则 UI 离线。
     * 网口：8s 无组播则 UI 离线（与 TCP 0x23 超时无关）。
     * 有 IP 但当前走串口、或从未收到组播时，按 0x23 判定（避免 LoRa 断流后主页一直在线）。
     */
    private fun pruneStaleProbeTelemetry() {
        val now = System.currentTimeMillis()
        for (probe in _savedProbes.value) {
            val probeId = probe.id
            if (_liveTelemetry.value[probeId]?.isOnline != true) continue

            val route = linkRouter.routeFor(probeId)
            val lastMcast = linkRouter.lastMulticastKeepaliveMillis(probeId)
            val useSerialStale =
                route == ProbeCommandLink.SERIAL ||
                    probe.ip.isBlank() ||
                    lastMcast == null

            if (useSerialStale) {
                val lastRx = linkRouter.lastRx23Millis(probeId)
                if (lastRx != null && now - lastRx < PROBE_STALE_MS) continue
                markProbeOffline(probeId, logTag = "0x23超时")
            } else {
                if (now - lastMcast!! < PROBE_STALE_MS) continue
                markProbeOffline(probeId, logTag = "组播超时")
            }
        }
    }

    /** 网口探头收到组播：UI 在线，保留最后一帧读数（不因 0x23 短暂中断变 ---） */
    private fun markNetworkProbeUiOnline(probeId: String) {
        val wasOnline = _liveTelemetry.value[probeId]?.isOnline == true
        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            map + (probeId to prev.copy(isOnline = true))
        }
        if (!wasOnline) {
            val name = _savedProbes.value.find { it.id == probeId }?.displayName ?: probeId
            appendAlertLog(
                message = "$name 已连接",
                kind = AlertLogKind.Connected,
            )
            sensorOfflineLogAggregator.onProbeConnected(probeId)
            doseAlarmLogAggregator.onProbeConnected(probeId)
        }
        if (_showSettings.value) {
            patchProbeRealtimeSummaryOnDraft(probeId)
        }
    }

    private fun markProbeOffline(probeId: String, logTag: String) {
        if (_liveTelemetry.value[probeId]?.isOnline != true) return
        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            map + (probeId to prev.asOffline())
        }
        val probe = _savedProbes.value.find { it.id == probeId }
        val name = probe?.displayName ?: probeId
        appendAlertLog(
            message = "$name 已断开",
            kind = AlertLogKind.Warning,
        )
        sensorOfflineLogAggregator.onProbeDisconnected(probeId)
        doseAlarmLogAggregator.onProbeDisconnected(probeId)
        recordProbeOfflineForTimeSync(probeId)
        if (_showSettings.value) {
            patchProbeOnlineOnDraft(probeId, false)
            patchProbeRealtimeSummaryOnDraft(probeId)
        }
        Log.i(ProbeConnectionManager.TAG, "探头离线($logTag) id=$probeId name=$name")
    }

    private fun onTcpFrame(probeId: String, frame: com.raydose.netshield.net.ParsedFsyFrame) {
        applyTelemetryFromFrame(probeId, frame, ProbeCommandLink.NETWORK)
    }

    private fun onProbeOnlineChanged(probeId: String, online: Boolean) {
        val probe = _savedProbes.value.find { it.id == probeId }
        val isNetwork = probe?.ip?.isNotBlank() == true

        if (isNetwork && !online) {
            Log.i(ProbeConnectionManager.TAG, "TCP 断开 probe=$probeId（UI 仍由组播判定）")
            if (_showSettings.value) {
                patchProbeOnlineOnDraft(probeId, false)
            }
            return
        }

        if (isNetwork && online) {
            _liveTelemetry.update { map ->
                val prev = map[probeId] ?: LiveProbeTelemetry()
                map + (probeId to prev.copy(isOnline = true))
            }
            if (_showSettings.value) {
                patchProbeOnlineOnDraft(probeId, true)
                val state = _settings.value
                val index = state.selectedProbeIndex
                if (index in state.manageDrafts.indices && state.manageDrafts[index].id == probeId) {
                    syncProbeCardAt(index)
                }
            }
            viewModelScope.launch {
                delay(2_000L)
                maybeAutoSyncTimeToProbe(probeId, reason = "online")
                delay(1_000L)
                requestFiveMinHistoryPull(probeId, hours = 24)
            }
            return
        }

        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            val updated = if (online) {
                prev.copy(isOnline = true)
            } else {
                prev.asOffline()
            }
            map + (probeId to updated)
        }
        val name = probe?.displayName ?: probeId
        appendAlertLog(
            message = if (online) "$name 已连接" else "$name 已断开",
            kind = if (online) AlertLogKind.Connected else AlertLogKind.Warning,
        )
        if (online) {
            sensorOfflineLogAggregator.onProbeConnected(probeId)
            doseAlarmLogAggregator.onProbeConnected(probeId)
            viewModelScope.launch {
                delay(2_000L)
                maybeAutoSyncTimeToProbe(probeId, reason = "online")
                delay(1_000L)
                requestFiveMinHistoryPull(probeId, hours = 24)
            }
        } else {
            sensorOfflineLogAggregator.onProbeDisconnected(probeId)
            doseAlarmLogAggregator.onProbeDisconnected(probeId)
            recordProbeOfflineForTimeSync(probeId)
        }
        if (_showSettings.value) {
            patchProbeOnlineOnDraft(probeId, online)
            if (!online) {
                patchProbeRealtimeSummaryOnDraft(probeId)
            }
            if (online) {
                val state = _settings.value
                val index = state.selectedProbeIndex
                if (index in state.manageDrafts.indices && state.manageDrafts[index].id == probeId) {
                    syncProbeCardAt(index)
                }
            }
        }
    }

    private fun patchProbeOnlineOnDraft(probeId: String, online: Boolean) {
        _settings.update { state ->
            val drafts = state.manageDrafts.map { d ->
                if (d.id != probeId) d else d.copy(isTcpOnline = online)
            }
            state.copy(manageDrafts = drafts)
        }
    }

    private fun patchProbeRealtimeSummaryOnDraft(probeId: String) {
        val telemetry = _liveTelemetry.value[probeId] ?: return
        _settings.update { state ->
            val drafts = state.manageDrafts.map { draft ->
                if (draft.id != probeId) {
                    draft
                } else {
                    val dose = if (telemetry.isOnline) telemetry.doseRateText else "---"
                    if (draft.isTcpOnline == telemetry.isOnline && draft.doseRateSummary == dose) {
                        draft
                    } else {
                        draft.copy(
                            isTcpOnline = telemetry.isOnline,
                            doseRateSummary = dose,
                        )
                    }
                }
            }
            state.copy(manageDrafts = drafts)
        }
    }

    /** 进入卡片后 fetch 的 0x13 应答：只更新当前翻页的那一张探头表单 */
    private fun applyConfigReadToSelectedProbeDraft(probeId: String) {
        val state = _settings.value
        val index = state.selectedProbeIndex
        if (index !in state.manageDrafts.indices) return
        if (state.manageDrafts[index].id != probeId) return
        val telemetry = _liveTelemetry.value[probeId]
        _settings.update { s ->
            if (index !in s.manageDrafts.indices) return@update s
            val drafts = s.manageDrafts.toMutableList()
            val merged = drafts[index].mergeConfigFromTelemetry(telemetry)
            drafts[index] = merged
            seedLastCommittedThresholds(probeId, merged)
            s.copy(manageDrafts = drafts)
        }
    }

    private fun appendAlertLog(
        message: String,
        kind: AlertLogKind,
        timestampMillis: Long = System.currentTimeMillis(),
    ) {
        val time = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
            .format(Date(timestampMillis))
        val entry = SystemAlertLog(
            id = nextAlertLogId++,
            timeText = time,
            message = message,
            kind = kind,
            timestampMillis = timestampMillis,
        )
        _alertLogs.update { current ->
            val next = (listOf(entry) + current).take(AlertLogRepository.MAX_LOGS)
            alertLogRepository.save(next)
            next
        }
    }

    fun clearAlertLogs() {
        _alertLogs.value = emptyList()
        alertLogRepository.clear()
    }

    private fun buildHomeState(
        saved: List<SavedProbe>,
        live: Map<String, LiveProbeTelemetry>,
        nowMillis: Long,
        timeDisplay: TimeSettings,
        flags: HomeUiFlags,
        logs: List<SystemAlertLog>,
        hostNetwork: HostNetworkSettings,
        hostAdapter: HostAdapterSnapshot,
        connectivity: HostConnectivityStatus,
        homeSelectedProbeId: String? = null,
    ): HomeUiState {
        val clock = HomeClockFormatter.format(java.util.Date(nowMillis), timeDisplay)
        val probes: List<SlaveProbeUi> = if (saved.isEmpty()) {
            listOf(
                SlaveProbeUi(
                    id = "placeholder",
                    name = "未添加探头",
                    isOnline = false,
                    doseRateText = "---",
                ),
            )
        } else {
            saved.map { probe ->
                probe.toSlaveProbeUi(live[probe.id])
            }
        }
        val selectedProbeIndex = homeSelectedProbeId
            ?.let { id -> probes.indexOfFirst { it.id == id } }
            ?.takeIf { it >= 0 }
            ?: 0
        return HomeUiState(
            dateText = clock.first,
            timeText = clock.second,
            hostNetwork = hostNetwork,
            hostEnvReadings = if (hostAdapter.hasData) {
                hostAdapter.envReadings
            } else {
                defaultHostEnvPlaceholders()
            },
            slaveProbes = probes,
            selectedProbeIndex = selectedProbeIndex,
            doorState = resolveDoorState(hostAdapter, live),
            alertLogs = logs,
            messages = emptyList(),
            statusBarExpanded = flags.statusBarExpanded,
            sideDrawerOpen = flags.sideDrawerOpen,
            bluetoothOnline = connectivity.bluetoothOnline,
            ethernetOnline = connectivity.ethernetOnline,
        )
    }

    private fun resolveDoorState(
        hostAdapter: HostAdapterSnapshot,
        live: Map<String, LiveProbeTelemetry>,
    ): DoorState {
        hostAdapter.doorOpen?.let { open ->
            return if (open) DoorState.Open else DoorState.Closed
        }
        return deriveDoorState(live)
    }

    override fun onCleared() {
        pauseAlarmExpiryJob?.cancel()
        cancelThresholdDebounceJobs()
        hostEnvSerialRepository.stop()
        hostAlarmController.release()
        connectionManager.stopDiscovery()
        connectionManager.disconnectAll()
        super.onCleared()
    }

    private data class HomeUiFlags(
        val statusBarExpanded: Boolean = false,
        val sideDrawerOpen: Boolean = false,
    )

    private data class HomeClockInputs(
        val saved: List<SavedProbe>,
        val live: Map<String, LiveProbeTelemetry>,
        val nowMillis: Long,
        val timeDisplay: TimeSettings,
    )

    private data class HomePanelInputs(
        val flags: HomeUiFlags,
        val logs: List<SystemAlertLog>,
        val hostNetwork: HostNetworkSettings,
        val hostAdapter: HostAdapterSnapshot,
        val connectivity: HostConnectivityStatus,
    )

    companion object {
        private const val DISCOVERY_TTL_MS = 30_000L
        /** 网口组播 / 串口 0x23：超过此时间无数据则 UI 离线（PHY 短抖约 2～4s） */
        private const val PROBE_STALE_MS = 8_000L
        /** 阈值输入框停止编辑后延迟下发（不依赖失焦） */
        private const val THRESHOLD_DEBOUNCE_MS = 800L
        /** 上线后自动 24h 五分钟历史补拉；暂时关闭，需要时改 true */
        private const val AUTO_FIVE_MIN_HISTORY_PULL = false
    }
}
