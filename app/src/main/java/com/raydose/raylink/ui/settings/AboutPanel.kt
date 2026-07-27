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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import com.raydose.raylink.R
import com.raydose.raylink.ui.localizeOtaProgress
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
private val AboutLabelWidth = 220.dp
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
    var updateHint by remember { mutableStateOf<String?>(null) }
    var pendingZjbUpgrade by remember { mutableStateOf<Pair<FileStorageLocation, FileListItem>?>(null) }
    var showSerialEditButton by remember { mutableStateOf(false) }
    var showSerialEditDialog by remember { mutableStateOf(false) }
    var serialClickCount by remember { mutableIntStateOf(0) }
    var serialClickWindowStart by remember { mutableLongStateOf(0L) }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    val zjbFwNotStarted = stringResource(R.string.zjb_fw_status_not_started)
    val zjbFwReading = stringResource(R.string.zjb_fw_reading)
    val zjbFwReadFailed = stringResource(R.string.zjb_fw_read_failed)
    val zjbFwPushSuccess = stringResource(R.string.zjb_fw_push_success)
    val zjbFwUpgradeFailed = stringResource(R.string.zjb_fw_upgrade_failed)
    val apkInstallRestarting = stringResource(R.string.apk_install_restarting)
    val apkInstallLaunchFailed = stringResource(R.string.apk_install_launch_failed)
    val apkPrepareFailed = stringResource(R.string.apk_prepare_failed)

    var zjbUpgradeStatus by remember { mutableStateOf(zjbFwNotStarted) }

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
            AboutInfoRow(
                stringResource(R.string.about_label_product_name),
                stringResource(R.string.about_product_name),
            )
            AboutInfoRow(stringResource(R.string.about_label_host_model), info.hostModel)
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
            zjbUpgradeStatus = zjbFwReading
            updateHint = null
            val bytesResult = withContext(Dispatchers.IO) {
                fileManagerRepository.readFirmwareBin(location, item.path)
            }
            bytesResult.onFailure { error ->
                isUpgradingZjb = false
                showMessage(error.message ?: zjbFwReadFailed)
                return@launch
            }
            val bytes = bytesResult.getOrThrow()
            onUpgradeZjbFirmware(bytes) { progress ->
                zjbUpgradeStatus = context.localizeOtaProgress(progress.statusText)
                zjbUpgradeProgress = progress.progress
            }.fold(
                onSuccess = {
                    showZjbUpdateDialog = false
                    pendingZjbUpgrade = null
                    showMessage(message = zjbFwPushSuccess, toast = true)
                },
                onFailure = { error ->
                    showMessage(error.message ?: zjbFwUpgradeFailed)
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
                                showMessage(message = apkInstallRestarting, toast = false)
                            },
                            onFailure = { error ->
                                showMessage(error.message ?: apkInstallLaunchFailed)
                            },
                        )
                    }.onFailure { error ->
                        showMessage(error.message ?: apkPrepareFailed)
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
            text = stringResource(R.string.about_label_host_serial),
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
                text = stringResource(R.string.action_edit),
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
                text = stringResource(R.string.about_label_host_serial),
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
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextPrimary, fontSize = 17.sp)
                }
                TextButton(onClick = { onSave(draft.trim()) }) {
                    Text(stringResource(R.string.action_save), color = RaylinkAccentBlue, fontSize = 17.sp)
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
            text = stringResource(R.string.about_label_software_version),
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
            text = stringResource(R.string.action_update),
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
            text = stringResource(R.string.about_label_hardware_version),
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
            text = stringResource(R.string.action_update),
            onClick = onUpdateClick,
            filled = true,
        )
    }
}
