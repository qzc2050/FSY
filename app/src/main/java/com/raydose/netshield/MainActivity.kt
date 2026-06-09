package com.raydose.netshield

import android.Manifest
import android.content.Intent
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.content.pm.PackageManager
import android.view.WindowInsets
import android.view.WindowInsetsController
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.SystemBarStyle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.data.HostSettingsRepository
import com.raydose.netshield.data.ProbeDoseHistoryRepository
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.model.AlbumMessage
import com.raydose.netshield.model.AlbumSettings
import com.raydose.netshield.model.MessageItem
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.MainViewModel
import com.raydose.netshield.ui.album.AlbumScreen
import com.raydose.netshield.ui.components.SideDrawerDestination
import com.raydose.netshield.ui.files.FileManagerScreen
import com.raydose.netshield.ui.files.FileManagerViewModel
import com.raydose.netshield.ui.home.HomeScreen
import com.raydose.netshield.ui.music.MusicScreen
import com.raydose.netshield.ui.music.MusicViewModel
import com.raydose.netshield.ui.probe.ProbeDetailScreen
import com.raydose.netshield.ui.settings.SettingsScreen
import com.raydose.netshield.ui.standby.StandbyIdleWatcher
import com.raydose.netshield.ui.standby.StandbyScreen
import com.raydose.netshield.ui.theme.NetShieldTheme
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()
    private val musicViewModel: MusicViewModel by viewModels()
    private val fileManagerViewModel: FileManagerViewModel by viewModels()

    /** 由 [StandbyIdleWatcher] 注册；[onUserInteraction] 时回调以重置空闲计时。 */
    private var onStandbyIdleUserInteraction: (() -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }
        hideSystemBars()
        setContent {
            NetShieldTheme {
                val homeState by viewModel.homeUiState.collectAsState()
                val settingsState by viewModel.settingsUiState.collectAsState()
                val showSettings by viewModel.settingsVisible.collectAsState()
                val systemTimeHint by viewModel.systemTimeHint.collectAsState()
                var showMusic by rememberSaveable { mutableStateOf(false) }
                var showAlbum by rememberSaveable { mutableStateOf(false) }
                var showFiles by rememberSaveable { mutableStateOf(false) }
                var showStandby by rememberSaveable { mutableStateOf(false) }
                var showProbeDetail by rememberSaveable { mutableStateOf(false) }
                var detailProbeId by rememberSaveable { mutableStateOf<String?>(null) }
                val hostSettingsRepository = remember { HostSettingsRepository(this) }
                val doseHistoryRepository = remember { ProbeDoseHistoryRepository(this) }
                val fileManagerRepository = remember { FileManagerRepository(this) }
                var usbGrantEpoch by remember { mutableStateOf(0) }
                var displaySoundSettings by remember { mutableStateOf(hostSettingsRepository.loadDisplaySound()) }
                var albumSettings by remember { mutableStateOf(hostSettingsRepository.loadAlbumSettings()) }
                var albumMessages by remember { mutableStateOf(hostSettingsRepository.loadAlbumMessages()) }
                var probeDetailOrgName by remember {
                    mutableStateOf(hostSettingsRepository.loadProbeDetailOrgName())
                }
                val saveAlbumSettings: (AlbumSettings) -> Unit = { settings ->
                    albumSettings = settings
                    hostSettingsRepository.saveAlbumSettings(settings)
                }
                val saveAlbumMessages: (List<AlbumMessage>) -> Unit = { messages ->
                    albumMessages = messages
                    hostSettingsRepository.saveAlbumMessages(messages)
                }
                val imagePickerLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.OpenDocument(),
                ) { uri ->
                    if (uri != null) {
                        runCatching {
                            contentResolver.takePersistableUriPermission(
                                uri,
                                Intent.FLAG_GRANT_READ_URI_PERMISSION,
                            )
                        }
                        saveAlbumSettings(albumSettings.copy(selectedImageUri = uri.toString()))
                    }
                }
                val usbTreeLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.OpenDocumentTree(),
                ) { uri ->
                    if (uri != null) {
                        runCatching {
                            contentResolver.takePersistableUriPermission(
                                uri,
                                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                            )
                        }
                        fileManagerRepository.saveUsbTreeUri(uri.toString())
                        fileManagerViewModel.grantUsbTree(uri.toString())
                        usbGrantEpoch++
                    }
                }
                var hasAudioPermission by remember { mutableStateOf(hasAudioReadPermission()) }
                val audioPermissionLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.RequestMultiplePermissions(),
                ) {
                    hasAudioPermission = hasAudioReadPermission()
                }
                val requestAudioPermission = {
                    val missing = requiredAudioPermissions()
                        .filter {
                            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
                        }
                        .toTypedArray()
                    if (missing.isEmpty()) {
                        hasAudioPermission = true
                    } else {
                        audioPermissionLauncher.launch(missing)
                    }
                }

                val isHomeScreen = !showStandby && !showProbeDetail && !showSettings &&
                    !showMusic && !showAlbum && !showFiles

                StandbyIdleWatcher(
                    watchIdle = isHomeScreen,
                    standbyMinutes = displaySoundSettings.standbyMinutes,
                    onRegisterUserInteraction = { listener ->
                        onStandbyIdleUserInteraction = listener
                    },
                    onEnterStandby = { showStandby = true },
                )

                if (showStandby) {
                    StandbyScreen(
                        state = homeState,
                        albumSettings = albumSettings,
                        messages = albumMessages,
                        timeSettings = settingsState.timeSettings,
                        probeCardMode = displaySoundSettings.probeCardMode,
                        onExit = { showStandby = false },
                    )
                } else if (showProbeDetail) {
                    ProbeDetailScreen(
                        probes = homeState.slaveProbes,
                        initialProbeId = detailProbeId,
                        organizationName = probeDetailOrgName,
                        onSaveOrganizationName = { orgName ->
                            probeDetailOrgName = orgName
                            hostSettingsRepository.saveProbeDetailOrgName(orgName)
                        },
                        onSaveIdentity = { probeId, name, location ->
                            viewModel.updateProbeIdentity(probeId, name, location)
                        },
                        fileManagerRepository = fileManagerRepository,
                        usbGrantEpoch = usbGrantEpoch,
                        onExportToPath = { probeId, storage, directoryPath ->
                            val probe = homeState.slaveProbes.find { it.id == probeId }
                            if (probe == null) {
                                "导出失败：未找到探头"
                            } else {
                                exportProbeSnapshotCsv(probe, storage, directoryPath, fileManagerRepository)
                            }
                        },
                        dailyDosesFor = { probeId, fallbackRate ->
                            doseHistoryRepository.dailySummaries(probeId, fallbackRate)
                        },
                        onBack = {
                            showProbeDetail = false
                            detailProbeId = null
                        },
                    )
                } else if (showSettings) {
                    SettingsScreen(
                        selectedTab = settingsState.selectedTab,
                        manageDrafts = settingsState.manageDrafts,
                        selectedProbeIndex = settingsState.selectedProbeIndex,
                        discoveredDevices = settingsState.discoveredDevices,
                        displaySound = settingsState.displaySound,
                        hostNetwork = settingsState.hostNetwork,
                        slaveNetworkCards = settingsState.slaveNetworkCards,
                        timeSettings = settingsState.timeSettings,
                        systemTimeHint = systemTimeHint,
                        aboutInfo = viewModel.aboutDeviceInfo(),
                        showAddProbeDialog = settingsState.showAddProbeDialog,
                        deleteConfirmProbeIndex = settingsState.deleteConfirmProbeIndex,
                        showSaveSuccessDialog = settingsState.showSaveSuccessDialog,
                        statusHint = settingsState.statusHint,
                        onBack = viewModel::closeSettings,
                        onTabSelected = viewModel::selectSettingsTab,
                        onProbePageSelected = viewModel::selectProbePage,
                        onDraftChange = viewModel::updateManageDraft,
                        onVolumeCommitted = viewModel::commitProbeVolume,
                        onDisplaySoundChange = viewModel::updateDisplaySound,
                        onBrightnessCommitted = viewModel::commitDisplaySoundBrightness,
                        onSystemVolumeCommitted = viewModel::commitDisplaySoundSystemVolume,
                        onSaveDisplaySound = {
                            viewModel.saveDisplaySoundSettings()
                            displaySoundSettings = settingsState.displaySound
                        },
                        onPreviewStandby = {
                            viewModel.closeSettings()
                            showStandby = true
                        },
                        onHostNetworkChange = viewModel::updateHostNetwork,
                        onSlaveNetworkChange = viewModel::updateSlaveNetworkCard,
                        onSaveHostNetwork = viewModel::saveHostNetworkSection,
                        onSaveSlaveNetwork = viewModel::saveSlaveNetworkSection,
                        onTimeSettingsChange = viewModel::updateTimeSettings,
                        onSyncTimeToDevice = viewModel::syncTimeToDevice,
                        onAddClick = viewModel::showAddProbeDialog,
                        onSaveClick = viewModel::saveProbeSettings,
                        onDismissAddDialog = viewModel::dismissAddProbeDialog,
                        onAddDevice = viewModel::addProbeFromDiscovery,
                        onDetailClick = { },
                        onDataDetailClick = { index ->
                            detailProbeId = settingsState.manageDrafts.getOrNull(index)?.id
                            if (detailProbeId != null) {
                                viewModel.closeSettings()
                                showProbeDetail = true
                            }
                        },
                        onRemoveProbe = viewModel::requestRemoveProbe,
                        onDismissDeleteConfirm = viewModel::dismissRemoveProbeConfirm,
                        onConfirmDeleteProbe = viewModel::confirmRemoveProbe,
                        onDismissSaveSuccess = viewModel::dismissSaveSuccessDialog,
                    )
                } else if (showMusic) {
                    MusicScreen(
                        viewModel = musicViewModel,
                        hasAudioPermission = hasAudioPermission,
                        probes = homeState.slaveProbes,
                        onRequestPermission = requestAudioPermission,
                        onBack = { showMusic = false },
                    )
                } else if (showAlbum) {
                    AlbumScreen(
                        probes = homeState.slaveProbes,
                        settings = albumSettings,
                        messages = albumMessages,
                        onSettingsChange = saveAlbumSettings,
                        onMessagesChange = saveAlbumMessages,
                        onPickImage = { imagePickerLauncher.launch(arrayOf("image/*")) },
                        onBack = { showAlbum = false },
                    )
                } else if (showFiles) {
                    FileManagerScreen(
                        viewModel = fileManagerViewModel,
                        probes = homeState.slaveProbes,
                        onRequestUsbAccess = { usbTreeLauncher.launch(null) },
                        onBack = { showFiles = false },
                    )
                } else {
                    val desktopMessages = if (albumSettings.applyMessageDesktop) {
                        albumMessages
                            .sortedByDescending { it.createdAtMillis }
                            .map { message -> MessageItem(message.id, message.text) }
                    } else {
                        emptyList()
                    }
                    HomeScreen(
                        state = homeState.copy(messages = desktopMessages),
                        probeCardMode = displaySoundSettings.probeCardMode,
                        onStatusBarToggle = viewModel::toggleStatusBar,
                        onSideDrawerToggle = { viewModel.setSideDrawerOpen(true) },
                        onSideDrawerDismiss = { viewModel.setSideDrawerOpen(false) },
                        onSideDrawerDestination = { dest ->
                            viewModel.setSideDrawerOpen(false)
                            when (dest) {
                                SideDrawerDestination.Music -> {
                                    showMusic = true
                                    if (!hasAudioPermission) {
                                        requestAudioPermission()
                                    }
                                }
                                SideDrawerDestination.Settings -> viewModel.openSettings()
                                SideDrawerDestination.Album -> showAlbum = true
                                SideDrawerDestination.Files -> showFiles = true
                            }
                        },
                        onStatusBarDismiss = { viewModel.setStatusBarExpanded(false) },
                        onProbeDetailClick = { probeId ->
                            detailProbeId = probeId
                            showProbeDetail = true
                        },
                        onMessageBarClick = { },
                    )
                }
            }
        }
    }

    override fun onUserInteraction() {
        super.onUserInteraction()
        onStandbyIdleUserInteraction?.invoke()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            hideSystemBars()
        }
    }

    private fun hideSystemBars() {
        window.insetsController?.let { controller ->
            controller.hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
            controller.systemBarsBehavior =
                WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun requiredAudioPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            arrayOf(Manifest.permission.READ_MEDIA_AUDIO)
        } else {
            arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE)
        }

    private fun hasAudioReadPermission(): Boolean =
        requiredAudioPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

    private fun exportProbeSnapshotCsv(
        probe: SlaveProbeUi,
        storage: FileStorageLocation,
        directoryPath: String,
        repository: FileManagerRepository,
    ): String {
        return runCatching {
            val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
            val safeName = probe.name.replace("\\s+".toRegex(), "_")
            val fileName = "probe_${safeName}_${stamp}.csv"
            val csv = buildProbeExportCsv(probe)
            val savedPath = repository.writeTextFile(storage, directoryPath, fileName, csv).getOrThrow()
            if (storage == FileStorageLocation.Usb) {
                "导出成功：$savedPath（已刷盘，可拔出 U 盘）"
            } else {
                "导出成功：$savedPath"
            }
        }.getOrElse { e ->
            "导出失败：${e.message ?: "未知错误"}"
        }
    }

    private fun buildProbeExportCsv(probe: SlaveProbeUi): String = buildString {
        appendLine("name,location,online,dose_rate,unit,temperature,pressure,humidity,co2,pm25,export_time")
        appendLine(
            listOf(
                probe.name,
                probe.location,
                probe.isOnline.toString(),
                probe.doseRateText,
                probe.doseUnit,
                probe.temperature,
                probe.pressure,
                probe.humidity,
                probe.co2,
                probe.pm25,
                SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()).format(Date()),
            ).joinToString(",") { value ->
                "\"${value.replace("\"", "\"\"")}\""
            },
        )
    }
}
