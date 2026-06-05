package com.raydose.netshield

import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.view.WindowInsets
import android.view.WindowInsetsController
import androidx.activity.SystemBarStyle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import com.raydose.netshield.ui.MainViewModel
import com.raydose.netshield.ui.components.SideDrawerDestination
import com.raydose.netshield.ui.home.HomeScreen
import com.raydose.netshield.ui.settings.SettingsScreen
import com.raydose.netshield.ui.theme.NetShieldTheme

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()

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

                if (showSettings) {
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
                        onSaveDisplaySound = viewModel::saveDisplaySoundSettings,
                        onPreviewStandby = viewModel::previewStandbyScreen,
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
                        onDataDetailClick = { },
                        onRemoveProbe = viewModel::requestRemoveProbe,
                        onDismissDeleteConfirm = viewModel::dismissRemoveProbeConfirm,
                        onConfirmDeleteProbe = viewModel::confirmRemoveProbe,
                        onDismissSaveSuccess = viewModel::dismissSaveSuccessDialog,
                    )
                } else {
                    HomeScreen(
                        state = homeState,
                        onStatusBarToggle = viewModel::toggleStatusBar,
                        onSideDrawerToggle = { viewModel.setSideDrawerOpen(true) },
                        onSideDrawerDismiss = { viewModel.setSideDrawerOpen(false) },
                        onSideDrawerDestination = { dest ->
                            viewModel.setSideDrawerOpen(false)
                            if (dest == SideDrawerDestination.Settings) {
                                viewModel.openSettings()
                            }
                        },
                        onStatusBarDismiss = { viewModel.setStatusBarExpanded(false) },
                        onProbeDetailClick = { },
                        onMessageBarClick = { },
                    )
                }
            }
        }
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
}
