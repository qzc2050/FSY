package com.raydose.raylink.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import android.widget.Toast
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.data.ZjbFirmwareRules
import com.raydose.raylink.data.ZjbOtaProgress
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private val AboutLabelSp = 20.sp
private val AboutValueSp = 19.sp
private val AboutLabelWidth = 176.dp
private const val SerialUnlockClickCount = 3
private const val SerialUnlockWindowMs = 2_000L

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
    onSaveHostSerial: (String) -> Unit,
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
    var showSerialEditButton by remember { mutableStateOf(false) }
    var showSerialEditDialog by remember { mutableStateOf(false) }
    var serialClickCount by remember { mutableIntStateOf(0) }
    var serialClickWindowStart by remember { mutableLongStateOf(0L) }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current

    fun showMessage(message: String, toast: Boolean = true) {
        updateHint = message
        if (toast) {
            Toast.makeText(context.applicationContext, message, Toast.LENGTH_LONG).show()
        }
    }

    fun onSerialLabelClick() {
        val now = System.currentTimeMillis()
        if (now - serialClickWindowStart > SerialUnlockWindowMs) {
            serialClickWindowStart = now
            serialClickCount = 1
        } else {
            serialClickCount += 1
        }
        if (serialClickCount >= SerialUnlockClickCount) {
            showSerialEditButton = true
            serialClickCount = 0
            serialClickWindowStart = 0L
        }
    }

    fun dismissSerialEditUi() {
        showSerialEditDialog = false
        showSerialEditButton = false
        serialClickCount = 0
        serialClickWindowStart = 0L
    }

    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        updateHint?.let { hint ->
            Text(
                text = hint,
                color = RaylinkAccentBlue,
                fontSize = 16.sp,
                modifier = Modifier.padding(bottom = 8.dp),
            )
        }
        if (isUpgradingZjb) {
            Text(
                text = zjbUpgradeStatus,
                color = RaylinkAccentBlue,
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
            AboutHostSerialRow(
                serial = info.serialNumber,
                showEditButton = showSerialEditButton,
                onLabelClick = ::onSerialLabelClick,
                onEditClick = { showSerialEditDialog = true },
            )
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

    if (showSerialEditDialog) {
        HostSerialEditDialog(
            initialSerial = info.serialNumber.takeIf { it != "—" }.orEmpty(),
            onDismiss = { dismissSerialEditUi() },
            onSave = { value ->
                onSaveHostSerial(value)
                dismissSerialEditUi()
            },
        )
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
                                    message = "正在安装更新，完成后将自动重启本应用。",
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
private fun AboutHostSerialRow(
    serial: String,
    showEditButton: Boolean,
    onLabelClick: () -> Unit,
    onEditClick: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = "主机序列号",
            width = AboutLabelWidth,
            fontSize = AboutLabelSp,
            maxLines = 1,
            softWrap = false,
            modifier = Modifier.clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onLabelClick,
            ),
        )
        Text(
            text = serial.ifBlank { "—" },
            color = RaylinkTextPrimary,
            fontSize = AboutValueSp,
            modifier = Modifier.weight(1f),
        )
        if (showEditButton) {
            SettingsInlineActionButton(
                text = "编辑",
                onClick = onEditClick,
                filled = true,
            )
        }
    }
}

@Composable
private fun HostSerialEditDialog(
    initialSerial: String,
    onDismiss: () -> Unit,
    onSave: (String) -> Unit,
) {
    var draft by remember { mutableStateOf(initialSerial) }
    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.55f)
                .clip(RoundedCornerShape(12.dp))
                .background(RaylinkSettingsEditorPanel)
                .padding(24.dp),
        ) {
            Text(
                text = "主机序列号",
                color = RaylinkTextPrimary,
                fontSize = 22.sp,
                fontWeight = FontWeight.SemiBold,
            )
            SettingsValueField(
                value = draft,
                onValueChange = { draft = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp),
            )
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 24.dp),
                horizontalArrangement = Arrangement.End,
            ) {
                TextButton(onClick = onDismiss) {
                    Text("取消", color = RaylinkTextPrimary, fontSize = 17.sp)
                }
                TextButton(onClick = { onSave(draft.trim()) }) {
                    Text("保存", color = RaylinkAccentBlue, fontSize = 17.sp)
                }
            }
        }
    }
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
            color = RaylinkTextPrimary,
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
            color = RaylinkTextPrimary,
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
