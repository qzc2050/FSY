package com.raydose.raylink.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.raydose.raylink.R
import com.raydose.raylink.ui.tr
import androidx.compose.ui.unit.sp
import com.raydose.raylink.model.DiscoveredDevice
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.model.DisplaySoundSettings
import com.raydose.raylink.model.HostNetworkSettings
import com.raydose.raylink.model.ProbeManageDraft
import com.raydose.raylink.model.SlaveNetworkCard
import com.raydose.raylink.model.TimeSettings
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkSettingsContentBg
import com.raydose.raylink.ui.theme.RaylinkSettingsNavBg
import com.raydose.raylink.ui.theme.RaylinkSettingsSummaryBg
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.ScreenSpec

@Composable
fun SettingsScreen(
    selectedTab: SettingsTab,
    manageDrafts: List<ProbeManageDraft>,
    selectedProbeIndex: Int,
    discoveredDevices: List<DiscoveredDevice>,
    displaySound: DisplaySoundSettings,
    hostNetwork: HostNetworkSettings,
    slaveNetworkCards: List<SlaveNetworkCard>,
    timeSettings: TimeSettings,
    systemTimeHint: String,
    aboutInfo: AboutDeviceInfo,
    showAddProbeDialog: Boolean,
    deleteConfirmProbeIndex: Int? = null,
    showSaveSuccessDialog: Boolean = false,
    statusHint: String? = null,
    onBack: () -> Unit,
    onTabSelected: (SettingsTab) -> Unit,
    onProbePageSelected: (Int) -> Unit,
    onDraftChange: (Int, ProbeManageDraft) -> Unit,
    onVolumeCommitted: (Int) -> Unit,
    onDisplaySoundChange: (DisplaySoundSettings) -> Unit,
    onBrightnessPreview: (Float) -> Unit,
    onBrightnessCommitted: () -> Unit,
    onSystemVolumeCommitted: () -> Unit,
    onHostAlarmVolumeCommitted: () -> Unit,
    onPromptVolumeCommitted: () -> Unit,
    onMuteCommitted: (Boolean) -> Unit,
    onPauseAlarmClick: () -> Unit,
    onSaveDisplaySound: () -> Unit,
    onPreviewStandby: () -> Unit,
    onCommitHostNetwork: (HostNetworkSettings) -> Unit,
    onCommitSlaveNetwork: (Int, SlaveNetworkCard) -> Unit,
    onFetchHostWifi: (
        (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit
    ) -> Unit = {},
    onFetchSlaveWifi: ((
        deviceId: Int,
        slaveIp: String,
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) -> Unit)? = null,
    onTimeSettingsChange: (TimeSettings) -> Unit,
    onSyncTimeToDevice: () -> Unit,
    onAddClick: () -> Unit,
    onSaveClick: () -> Unit,
    onDismissAddDialog: () -> Unit,
    onAddDevice: (DiscoveredDevice) -> Unit,
    onDataDetailClick: (Int) -> Unit,
    onRemoveProbe: (Int) -> Unit,
    onDismissDeleteConfirm: () -> Unit,
    onConfirmDeleteProbe: () -> Unit,
    onDismissSaveSuccess: () -> Unit,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onRequestUsbAccess: () -> Unit,
    onInstallApk: (java.io.File) -> Result<Unit>,
    onUpgradeZjbFirmware: suspend (ByteArray, (com.raydose.raylink.data.ZjbOtaProgress) -> Unit) -> Result<Unit>,
    onUpgradeProbeFirmware: suspend (
        probeId: String,
        fileBytes: ByteArray,
        onProgress: (com.raydose.raylink.data.ZjbOtaProgress) -> Unit,
    ) -> Result<Unit>,
    onSaveHostSerial: (String) -> Unit = {},
) {
    val context = LocalContext.current
    val resolvedFetchSlaveWifi = onFetchSlaveWifi ?: { _, _, onDone ->
        onDone(false, context.tr(R.string.settings_slave_fetch_not_impl), null, null)
    }
    val probeFallbackName = stringResource(R.string.settings_probe_fallback_name)

    Box(modifier = Modifier.fillMaxSize()) {
        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
            val navRailWidth = maxWidth * SettingsLayout.NAV_RAIL_WIDTH_FRACTION
            Column(modifier = Modifier.fillMaxSize()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(summaryHeight)
                        .background(RaylinkSettingsSummaryBg),
                ) {
                    SettingsTopProbeStatus(drafts = manageDrafts)
                    IconButton(
                        onClick = onBack,
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(top = 8.dp, end = 12.dp),
                    ) {
                        Text("✕", color = RaylinkTextPrimary, fontSize = 28.sp)
                    }
                }
                Row(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth(),
                ) {
                    SettingsNavRail(
                        selectedTab = selectedTab,
                        onTabSelected = onTabSelected,
                        navRailWidth = navRailWidth,
                        modifier = Modifier.background(RaylinkSettingsNavBg),
                    )
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .background(RaylinkSettingsContentBg),
                    ) {
                        Column(modifier = Modifier.fillMaxSize()) {
                            if (statusHint != null && selectedTab != SettingsTab.Probes) {
                                Text(
                                    text = statusHint,
                                    color = RaylinkAccentBlue,
                                    fontSize = 15.sp,
                                    modifier = Modifier.padding(horizontal = 28.dp, vertical = 8.dp),
                                )
                            }
                            when (selectedTab) {
                        SettingsTab.DisplaySound -> DisplaySoundPanel(
                            settings = displaySound,
                            onChange = onDisplaySoundChange,
                            onBrightnessPreview = onBrightnessPreview,
                            onBrightnessCommitted = onBrightnessCommitted,
                            onSystemVolumeCommitted = onSystemVolumeCommitted,
                            onHostAlarmVolumeCommitted = onHostAlarmVolumeCommitted,
                            onPromptVolumeCommitted = onPromptVolumeCommitted,
                            onMuteCommitted = onMuteCommitted,
                            onPauseAlarmClick = onPauseAlarmClick,
                            onSaveClick = onSaveDisplaySound,
                            onPreviewStandby = onPreviewStandby,
                            modifier = Modifier.weight(1f),
                        )
                                SettingsTab.Network -> NetworkSettingsPanel(
                                    host = hostNetwork,
                                    slaves = slaveNetworkCards,
                                    onSaveHost = onCommitHostNetwork,
                                    onSaveSlave = onCommitSlaveNetwork,
                                    onFetchHostWifi = onFetchHostWifi,
                                    onFetchSlaveWifi = resolvedFetchSlaveWifi,
                                    modifier = Modifier.weight(1f),
                                )
                                SettingsTab.Time -> TimeSettingsPanel(
                                    settings = timeSettings,
                                    systemTimeHint = systemTimeHint,
                                    onChange = onTimeSettingsChange,
                                    onSyncToDevice = onSyncTimeToDevice,
                                    modifier = Modifier.weight(1f),
                                )
                                SettingsTab.Probes -> ProbeManagePanel(
                                    manageDrafts = manageDrafts,
                                    selectedProbeIndex = selectedProbeIndex,
                                    statusHint = statusHint,
                                    showSaveSuccess = showSaveSuccessDialog,
                                    onDismissSaveSuccess = onDismissSaveSuccess,
                                    onProbePageSelected = onProbePageSelected,
                                    onDraftChange = onDraftChange,
                                    onVolumeCommitted = onVolumeCommitted,
                                    onAddClick = onAddClick,
                                    onSaveClick = onSaveClick,
                                    onDataDetailClick = onDataDetailClick,
                                    onRemoveProbe = onRemoveProbe,
                                    fileManagerRepository = fileManagerRepository,
                                    usbGrantEpoch = usbGrantEpoch,
                                    onRequestUsbAccess = onRequestUsbAccess,
                                    onUpgradeProbeFirmware = onUpgradeProbeFirmware,
                                    modifier = Modifier.weight(1f),
                                )
                                SettingsTab.About -> AboutPanel(
                                    info = aboutInfo,
                                    fileManagerRepository = fileManagerRepository,
                                    usbGrantEpoch = usbGrantEpoch,
                                    onRequestUsbAccess = onRequestUsbAccess,
                                    onInstallApk = onInstallApk,
                                    onUpgradeZjbFirmware = onUpgradeZjbFirmware,
                                    onSaveHostSerial = onSaveHostSerial,
                                    modifier = Modifier.weight(1f),
                                )
                            }
                        }
                        if (showSaveSuccessDialog && selectedTab in setOf(SettingsTab.DisplaySound, SettingsTab.Network)) {
                            Box(
                                modifier = Modifier.fillMaxSize(),
                                contentAlignment = Alignment.Center,
                            ) {
                                SaveSuccessToast(onDismiss = onDismissSaveSuccess)
                            }
                        }
                    }
                }
            }
        }
        if (showAddProbeDialog) {
            AddProbeDialog(
                discovered = discoveredDevices,
                draftProbes = manageDrafts.map { it.savedProbe },
                onDismiss = onDismissAddDialog,
                onAdd = onAddDevice,
            )
        }
        deleteConfirmProbeIndex?.let { index ->
            val name = manageDrafts.getOrNull(index)?.displayName ?: probeFallbackName
            DeleteProbeConfirmDialog(
                probeName = name,
                onDismiss = onDismissDeleteConfirm,
                onConfirm = onConfirmDeleteProbe,
            )
        }
    }
}
