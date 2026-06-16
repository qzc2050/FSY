package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.data.FileManagerRepository
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
    modifier: Modifier = Modifier,
) {
    var showUpdateDialog by remember { mutableStateOf(false) }
    var isInstalling by remember { mutableStateOf(false) }
    var updateHint by remember { mutableStateOf<String?>(null) }
    val scope = rememberCoroutineScope()

    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        updateHint?.let { hint ->
            Text(
                text = hint,
                color = NetShieldAccentBlue,
                fontSize = 16.sp,
                modifier = Modifier.padding(bottom = 8.dp),
            )
        }
        SettingsCard {
            AboutInfoRow("产品名称", info.productName)
            AboutInfoRow("主机型号", info.hostModel)
            AboutInfoRow("主机序列号", info.serialNumber)
            AboutSoftwareVersionRow(
                version = info.softwareVersion,
                onUpdateClick = { showUpdateDialog = true },
            )
            AboutInfoRow("硬件版本", info.hardwareVersion)
        }
    }

    if (showUpdateDialog) {
        ApkUpdatePickerDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            isInstalling = isInstalling,
            onDismiss = {
                if (!isInstalling) showUpdateDialog = false
            },
            onRequestUsbAccess = onRequestUsbAccess,
            onInstall = { location, path ->
                scope.launch {
                    isInstalling = true
                    updateHint = null
                    val staged = withContext(Dispatchers.IO) {
                        fileManagerRepository.stageApkForInstall(location, path)
                    }
                    isInstalling = false
                    staged.onSuccess { apkFile ->
                        onInstallApk(apkFile).fold(
                            onSuccess = {
                                showUpdateDialog = false
                                updateHint = "已调起系统安装，请按提示完成更新"
                            },
                            onFailure = { error ->
                                updateHint = error.message ?: "调起安装失败"
                            },
                        )
                    }.onFailure { error ->
                        updateHint = error.message ?: "准备安装包失败"
                    }
                }
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
