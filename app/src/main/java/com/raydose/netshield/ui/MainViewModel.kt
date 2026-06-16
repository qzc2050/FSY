package com.raydose.netshield.ui

import android.app.Application
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.netshield.data.DisplaySoundController
import com.raydose.netshield.data.HostAlarmController
import com.raydose.netshield.data.HostEnvSerialRepository
import com.raydose.netshield.data.HostSettingsRepository
import com.raydose.netshield.ui.home.HomeClockFormatter
import com.raydose.netshield.data.ProbeConfigRepository
import com.raydose.netshield.data.ProbeConnectionManager
import com.raydose.netshield.data.ProbeDoseHistoryRepository
import com.raydose.netshield.model.DisplaySoundSettings
import com.raydose.netshield.model.PAUSE_ALARM_DURATION_MS
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.isHostAlarmSuppressed
import com.raydose.netshield.model.withExpiredPauseCleared
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.model.TimeSettings
import com.raydose.netshield.net.listFsyNetworkOptions
import com.raydose.netshield.ui.settings.AboutDeviceInfo
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.TabletFormFactor
import com.raydose.netshield.model.AlertLogKind
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.DoorState
import com.raydose.netshield.model.HostAdapterSnapshot
import com.raydose.netshield.model.defaultHostEnvPlaceholders
import com.raydose.netshield.model.HomeUiState
import com.raydose.netshield.model.LiveProbeTelemetry
import com.raydose.netshield.model.ProbeManageDraft
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.model.SystemAlertLog
import com.raydose.netshield.model.applyParsedFrame
import com.raydose.netshield.model.buildControlBit2WriteFrame
import com.raydose.netshield.model.buildLowerAlarmCheckboxWriteFrames
import com.raydose.netshield.model.buildUpperAlarmCheckboxWriteFrames
import com.raydose.netshield.model.buildVolumeWriteFrame
import com.raydose.netshield.model.mergeControlBit2Enables
import com.raydose.netshield.model.deriveDoorState
import com.raydose.netshield.model.matchesSaved
import com.raydose.netshield.model.mergeConfigFromTelemetry
import com.raydose.netshield.model.toManageDraft
import com.raydose.netshield.model.toSlaveProbeUi
import com.raydose.netshield.ui.settings.SettingsTab
import com.raydose.netshield.net.parseFsyBroadcast
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
    private val displaySoundController = DisplaySoundController(application)
    private val hostAlarmController = HostAlarmController(application)
    private val hostEnvSerialRepository = HostEnvSerialRepository()
    private var windowBrightnessApplier: ((Float) -> Unit)? = null
    private var pauseAlarmExpiryJob: Job? = null
    private val discoveredMap = ConcurrentHashMap<String, DiscoveredDevice>()
    private var nextAlertLogId = 1L

    private val _savedProbes = MutableStateFlow(repository.load())
    private val _liveTelemetry = MutableStateFlow<Map<String, LiveProbeTelemetry>>(emptyMap())
    private val _nowMillis = MutableStateFlow(System.currentTimeMillis())
    private val _timeDisplaySettings = MutableStateFlow(hostSettingsRepository.loadTimeSettings())
    private val _uiFlags = MutableStateFlow(HomeUiFlags())
    private val _settings = MutableStateFlow(ProbeSettingsUiState())
    private val _hostNetwork = MutableStateFlow(hostSettingsRepository.loadHostNetwork())
    private val _alertLogs = MutableStateFlow<List<SystemAlertLog>>(emptyList())
    private val _displaySoundSettings = MutableStateFlow(hostSettingsRepository.loadDisplaySound())

    private val connectionManager = ProbeConnectionManager(
        context = application,
        onDiscoveredRaw = ::onDiscoveryDatagram,
        onTcpFrame = ::onTcpFrame,
        onProbeOnlineChanged = ::onProbeOnlineChanged,
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
        _uiFlags,
        _alertLogs,
        _hostNetwork,
        hostEnvSerialRepository.snapshot,
    ) { clockInputs, flags, logs, hostNetwork, hostAdapter ->
        buildHomeState(
            clockInputs.saved,
            clockInputs.live,
            clockInputs.nowMillis,
            clockInputs.timeDisplay,
            flags,
            logs,
            hostNetwork,
            hostAdapter,
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
            connectionManager.reconnectAll(saved)
            viewModelScope.launch {
                delay(1500L)
                connectionManager.setSavedProbes(_savedProbes.value)
                connectionManager.reconnectAll(_savedProbes.value)
            }
        }
        viewModelScope.launch {
            while (true) {
                _nowMillis.value = System.currentTimeMillis()
                pruneStaleDiscovery()
                delay(1000L)
            }
        }
        schedulePauseAlarmExpiry(_displaySoundSettings.value.pauseAlarmUntilMillis)
        hostEnvSerialRepository.start()
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
        val loadedTime = hostSettingsRepository.loadTimeSettings()
        _settings.value = ProbeSettingsUiState(
            selectedTab = SettingsTab.DisplaySound,
            selectedProbeIndex = 0,
            manageDrafts = probes.map { it.toManageDraft() },
            draftProbes = probes,
            discoveredDevices = discoveredMap.values.sortedByDescending { it.lastSeenMillis },
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
            else -> Unit
        }
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
        val hint = systemTimeDisplayText()
        _settings.update { it.copy(statusHint = "同步到设备待协议联调（从机时间写入）") }
        Log.i(ProbeConnectionManager.TAG, "同步时间到设备（待实现） at=$hint")
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
        val formFactor = ScreenSpec.formFactor(app.resources.configuration.screenWidthDp)
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
            productName = "NetShield 联盾环境辐射监测系统 控制主机",
            hostModel = model,
            serialNumber = serial,
            softwareVersion = version,
            hardwareVersion = "v1.0.0",
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

    /** 进入设置页或切换探头卡片时：从遥测合并一次并主动读 0x52/7A/7B/阈值；停留期间不再被 0x23 刷新表单 */
    private fun syncProbeCardAt(index: Int) {
        val state = _settings.value
        if (index !in state.manageDrafts.indices) return
        val probe = state.draftProbes.getOrNull(index) ?: return
        val telemetry = _liveTelemetry.value[probe.id]
        _settings.update { s ->
            if (index !in s.manageDrafts.indices) return@update s
            val drafts = s.manageDrafts.toMutableList()
            drafts[index] = drafts[index]
                .mergeConfigFromTelemetry(telemetry)
                .copy(isTcpOnline = telemetry?.isOnline == true)
            s.copy(manageDrafts = drafts)
        }
        if (telemetry?.isOnline == true) {
            connectionManager.fetchManageConfig(probe)
        }
    }

    /** 音量滑条松手后写入 0x7A */
    fun commitProbeVolume(index: Int) {
        val draft = _settings.value.manageDrafts.getOrNull(index) ?: return
        if (!draft.isTcpOnline) return
        connectionManager.sendFrames(draft.id, listOf(draft.buildVolumeWriteFrame()))
        _liveTelemetry.update { map ->
            val t = map[draft.id] ?: LiveProbeTelemetry()
            map + (draft.id to t.copy(volume = draft.volume))
        }
        Log.d(ProbeConnectionManager.TAG, "音量已写入 probe=${draft.id} reg=0x7A")
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
            pushDraftWritesIfNeeded(prev, draft)
        }
    }

    /**
     * 仅 checkbox 变化时写从机（阈值/音量改字只改草稿，不发命令）：
     * - 上限旁「报警」→ 0x52（bit0）+ 0x32 当前上限值
     * - 下限旁「报警」→ 0x52（bit1）+ 0x34 当前下限值（与上限独立）
     * - 从机屏幕 / 报警灯光 → 0x7B bit14/13
     */
    private fun pushDraftWritesIfNeeded(prev: ProbeManageDraft, draft: ProbeManageDraft) {
        if (!draft.isTcpOnline) return
        val probeId = draft.id
        val liveCtrl = _liveTelemetry.value[probeId]?.controlBit2Value
        val frames = mutableListOf<ByteArray>()

        if (prev.slaveScreenOn != draft.slaveScreenOn || prev.alarmLightOn != draft.alarmLightOn) {
            frames += draft.buildControlBit2WriteFrame(liveCtrl)
            val merged = mergeControlBit2Enables(liveCtrl ?: 0L, draft.slaveScreenOn, draft.alarmLightOn)
            _liveTelemetry.update { map ->
                val t = map[probeId] ?: LiveProbeTelemetry()
                map + (probeId to t.copy(
                    controlBit2Value = merged,
                    slaveScreenOn = draft.slaveScreenOn,
                    alarmLightOn = draft.alarmLightOn,
                ))
            }
        }
        if (prev.radiationUpperAlarmOn != draft.radiationUpperAlarmOn) {
            frames += draft.buildUpperAlarmCheckboxWriteFrames()
            patchTelemetryAlarmEnable(probeId, draft)
        }
        if (prev.radiationLowerAlarmOn != draft.radiationLowerAlarmOn) {
            frames += draft.buildLowerAlarmCheckboxWriteFrames()
            patchTelemetryAlarmEnable(probeId, draft)
        }
        if (frames.isNotEmpty()) {
            connectionManager.sendFrames(probeId, frames)
            Log.d(ProbeConnectionManager.TAG, "探头 checkbox 写入 probe=$probeId frames=${frames.size}")
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

    fun closeSettings() {
        _showSettings.value = false
        _settings.update { it.copy(showAddProbeDialog = false, showSaveSuccessDialog = false) }
        val savedBrightness = hostSettingsRepository.loadDisplaySound().brightness
        windowBrightnessApplier?.invoke(savedBrightness)
    }

    fun showAddProbeDialog() {
        _settings.update {
            it.copy(
                showAddProbeDialog = true,
                discoveredDevices = discoveredMap.values.sortedByDescending { it.lastSeenMillis },
                statusHint = if (discoveredMap.isEmpty()) "正在搜索组播设备…" else null,
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
        connectionManager.connect(added)
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
        val broadcast = parseFsyBroadcast(text) ?: return
        val device = DiscoveredDevice.fromBroadcast(broadcast)
        discoveredMap[device.stableId] = device
        if (_settings.value.showAddProbeDialog) {
            _settings.update {
                it.copy(
                    discoveredDevices = discoveredMap.values.sortedByDescending { d -> d.lastSeenMillis },
                    statusHint = null,
                )
            }
        }
    }

    private fun pruneStaleDiscovery() {
        val cutoff = System.currentTimeMillis() - DISCOVERY_TTL_MS
        val removed = discoveredMap.entries.removeIf { it.value.lastSeenMillis < cutoff }
        if (removed && _settings.value.showAddProbeDialog) {
            _settings.update {
                it.copy(discoveredDevices = discoveredMap.values.sortedByDescending { d -> d.lastSeenMillis })
            }
        }
    }

    private fun onTcpFrame(probeId: String, frame: com.raydose.netshield.net.ParsedFsyFrame) {
        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            val next = prev.applyParsedFrame(frame)
            frame.uploadValues?.takeIf { it.size >= 8 }?.let { values ->
                val dose = values[0] / 100.0
                doseHistoryRepository.recordSampleIfDue(probeId, dose)
            }
            map + (probeId to next)
        }
        // 设置页顶部辐射量摘要需要在任意子页实时刷新，不仅限探头管理页。
        if (_showSettings.value) {
            patchProbeRealtimeSummaryOnDraft(probeId)
        }
        // 仅主动读应答 0x13 刷新当前卡片一次；0x23 不刷新表单
        if (frame.func == 0x13 && _showSettings.value) {
            applyConfigReadToSelectedProbeDraft(probeId)
        }
    }

    private fun onProbeOnlineChanged(probeId: String, online: Boolean) {
        _liveTelemetry.update { map ->
            val prev = map[probeId] ?: LiveProbeTelemetry()
            val updated = if (online) {
                prev.copy(isOnline = true)
            } else {
                LiveProbeTelemetry(isOnline = false)
            }
            map + (probeId to updated)
        }
        val probe = _savedProbes.value.find { it.id == probeId }
        val name = probe?.displayName ?: probeId
        appendAlertLog(
            message = if (online) "$name 已连接" else "$name 已断开",
            kind = if (online) AlertLogKind.Connected else AlertLogKind.Warning,
        )
        if (_showSettings.value) {
            patchProbeOnlineOnDraft(probeId, online)
            // 添加探头或重连后 TCP 才就绪：此时补读 0x40/0x52/0x7A/0x7B（添加时 sync 可能因未在线而跳过）
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
            drafts[index] = drafts[index].mergeConfigFromTelemetry(telemetry)
            s.copy(manageDrafts = drafts)
        }
    }

    private fun appendAlertLog(message: String, kind: AlertLogKind) {
        val time = java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss", java.util.Locale.getDefault())
            .format(java.util.Date())
        val entry = SystemAlertLog(id = nextAlertLogId++, timeText = time, message = message, kind = kind)
        _alertLogs.update { (listOf(entry) + it).take(100) }
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
            doorState = resolveDoorState(hostAdapter, live),
            alertLogs = logs,
            messages = emptyList(),
            statusBarExpanded = flags.statusBarExpanded,
            sideDrawerOpen = flags.sideDrawerOpen,
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

    companion object {
        private const val DISCOVERY_TTL_MS = 30_000L
    }
}
