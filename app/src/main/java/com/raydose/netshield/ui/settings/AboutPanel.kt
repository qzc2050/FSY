package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import android.widget.Toast
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.data.ZjbFirmwareRules
import com.raydose.netshield.data.ZjbOtaProgress
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private val AboutLabelSp = 20.sp
private val AboutValueSp = 19.sp
private val AboutLabelWidth = 176.dp

data class AboutDeviceInfo(
    val productName: String,
    val hostModel: String,
    val serialNumber: String,
    val softwareVersion: String,
    val hardwareVersion: String,
)

@Composable
fun AboutPanel(
    info: AboutDeviceInfo,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onRequestUsbAccess: () -> Unit,
    onInstallApk: (File) -> Result<Unit>,
    onUpgradeZjbFirmware: suspend (ByteArray, (ZjbOtaProgress) -> Unit) -> Result<Unit>,
    modifier: Modifier = Modifier,
) {
    var showApkUpdateDialog by remember { mutableStateOf(false) }
    var showZjbUpdateDialog by remember { mutableStateOf(false) }
    var isInstallingApk by remember { mutableStateOf(false) }
    var isUpgradingZjb by remember { mutableStateOf(false) }
    var zjbUpgradeProgress by remember { mutableFloatStateOf(0f) }
    var zjbUpgradeStatus by remember { mutableStateOf("未开始") }
    var updateHint by remember { mutableStateOf<String?>(null) }
    var pendingZjbUpgrade by remember { mutableStateOf<Pair<FileStorageLocation, FileListItem>?>(null) }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current

    fun showMessage(message: String, toast: Boolean = true) {
        updateHint = message
        if (toast) {
            Toast.makeText(context.applicationContext, message, Toast.LENGTH_LONG).show()
        }
    }

    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        updateHint?.let { hint ->
            Text(
                text = hint,
                color = NetShieldAccentBlue,
                fontSize = 16.sp,
                modifier = Modifier.padding(bottom = 8.dp),
            )
        }
        if (isUpgradingZjb) {
            Text(
                text = zjbUpgradeStatus,
                color = NetShieldAccentBlue,
                fontSize = 16.sp,
            )
            LinearProgressIndicator(
                progress = { zjbUpgradeProgress.coerceIn(0f, 1f) },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 8.dp),
            )
        }
        SettingsCard {
            AboutInfoRow("产品名称", info.productName)
            AboutInfoRow("主机型号", info.hostModel)
            AboutInfoRow("主机序列号", info.serialNumber)
            AboutSoftwareVersionRow(
                version = info.softwareVersion,
                onUpdateClick = { showApkUpdateDialog = true },
            )
            AboutHardwareVersionRow(
                version = info.hardwareVersion,
                onUpdateClick = { showZjbUpdateDialog = true },
            )
        }
    }

    fun startZjbUpgrade(location: FileStorageLocation, item: FileListItem) {
        scope.launch {
            isUpgradingZjb = true
            zjbUpgradeProgress = 0f
            zjbUpgradeStatus = "读取固件文件..."
            updateHint = null
            val bytesResult = withContext(Dispatchers.IO) {
                fileManagerRepository.readFirmwareBin(location, item.path)
            }
            bytesResult.onFailure { error ->
                isUpgradingZjb = false
                showMessage(error.message ?: "读取固件失败")
                return@launch
            }
            val bytes = bytesResult.getOrThrow()
            onUpgradeZjbFirmware(bytes) { progress ->
                zjbUpgradeStatus = progress.statusText
                zjbUpgradeProgress = progress.progress
            }.fold(
                onSuccess = {
                    showZjbUpdateDialog = false
                    pendingZjbUpgrade = null
                    showMessage(
                        message = "转接板固件已推送，设备将校验并重启。重启后请返回本页确认硬件版本。",
                        toast = true,
                    )
                },
                onFailure = { error ->
                    showMessage(error.message ?: "转接板固件升级失败")
                },
            )
            isUpgradingZjb = false
        }
    }

    if (pendingZjbUpgrade != null) {
        val (location, item) = pendingZjbUpgrade!!
        ZjbFirmwareConfirmDialog(
            fileName = item.name,
            sizeBytes = item.sizeBytes,
            onDismiss = { pendingZjbUpgrade = null },
            onConfirm = {
                pendingZjbUpgrade = null
                startZjbUpgrade(location, item)
            },
        )
    }

    if (showApkUpdateDialog) {
        ApkUpdatePickerDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            isInstalling = isInstallingApk,
            onDismiss = {
                if (!isInstallingApk) showApkUpdateDialog = false
            },
            onRequestUsbAccess = onRequestUsbAccess,
            onInstall = { location, path ->
                scope.launch {
                    isInstallingApk = true
                    updateHint = null
                    val staged = withContext(Dispatchers.IO) {
                        fileManagerRepository.stageApkForInstall(location, path)
                    }
                    isInstallingApk = false
                    staged.onSuccess { apkFile ->
                        onInstallApk(apkFile).fold(
                            onSuccess = {
                                showApkUpdateDialog = false
                                showMessage(
                                    message = "已调起系统安装。确认后可能短暂看到系统桌面，安装完成将自动返回本应用。",
                                    toast = false,
                                )
                            },
                            onFailure = { error ->
                                showMessage(error.message ?: "调起安装失败")
                            },
                        )
                    }.onFailure { error ->
                        showMessage(error.message ?: "准备安装包失败")
                    }
                }
            },
        )
    }

    if (showZjbUpdateDialog) {
        ZjbFirmwareUpdateDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            isUpgrading = isUpgradingZjb,
            upgradeProgress = zjbUpgradeProgress,
            upgradeStatus = zjbUpgradeStatus,
            onDismiss = {
                if (!isUpgradingZjb) showZjbUpdateDialog = false
            },
            onRequestUsbAccess = onRequestUsbAccess,
            onUpgrade = { location, item ->
                if (!ZjbFirmwareRules.isValidSelection(item.name, item.sizeBytes)) {
                    showMessage(ZjbFirmwareRules.REJECT_MESSAGE)
                    return@ZjbFirmwareUpdateDialog
                }
                pendingZjbUpgrade = location to item
            },
        )
    }
}

@Composable
private fun AboutInfoRow(label: String, value: String) {
    SettingsReadOnlyHalfRow(
        label = label,
        value = value,
        labelFontSize = AboutLabelSp,
        valueFontSize = AboutValueSp,
        labelWidth = AboutLabelWidth,
        labelSingleLine = true,
    )
}

@Composable
private fun AboutSoftwareVersionRow(
    version: String,
    onUpdateClick: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = "软件版本",
            width = AboutLabelWidth,
            fontSize = AboutLabelSp,
            maxLines = 1,
            softWrap = false,
        )
        Text(
            text = version.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = AboutValueSp,
            modifier = Modifier.weight(1f),
        )
        SettingsInlineActionButton(
            text = "更新",
            onClick = onUpdateClick,
            filled = true,
        )
    }
}

@Composable
private fun AboutHardwareVersionRow(
    version: String,
    onUpdateClick: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = "硬件版本",
            width = AboutLabelWidth,
            fontSize = AboutLabelSp,
            maxLines = 1,
            softWrap = false,
        )
        Text(
            text = version.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = AboutValueSp,
            modifier = Modifier.weight(1f),
        )
        SettingsInlineActionButton(
            text = "更新",
            onClick = onUpdateClick,
            filled = true,
        )
    }
}
