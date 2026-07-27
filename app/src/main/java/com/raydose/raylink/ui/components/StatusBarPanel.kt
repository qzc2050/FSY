package com.raydose.raylink.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.model.AlertLogKind
import com.raydose.raylink.model.ConnectedDeviceUi
import com.raydose.raylink.model.SystemAlertLog
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.ScreenSpec
import com.raydose.raylink.ui.theme.rememberTabletFormFactor

/**
 * 下拉面板纵向三行（image11）：
 * ① 主机 WiFi/蓝牙（整行居中） ② 在线设备（已入库探头 + 未入库本公司设备，横向） ③ 日志（纵向滚动）
 */
@Composable
fun StatusBarPanelBody(
    wifiName: String,
    wifiPassword: String,
    bluetoothName: String,
    connectedDevices: List<ConnectedDeviceUi>,
    alertLogs: List<SystemAlertLog>,
    onClearAlertLogs: () -> Unit = {},
    modifier: Modifier = Modifier,
) {
    val formFactor = rememberTabletFormFactor()
    val infoLabelSp = ScreenSpec.statusBarInfoLabelSp(formFactor).sp
    val infoValueSp = ScreenSpec.statusBarInfoValueSp(formFactor).sp
    val deviceNameSp = ScreenSpec.statusBarDeviceNameSp(formFactor).sp
    val deviceIpSp = ScreenSpec.statusBarDeviceIpSp(formFactor).sp
    val logTimeSp = ScreenSpec.statusBarLogTimeSp(formFactor).sp
    val logMessageSp = ScreenSpec.statusBarLogMessageSp(formFactor).sp
    val placeholderSp = ScreenSpec.statusBarPlaceholderSp(formFactor).sp
    val logIconSize = ScreenSpec.statusBarLogIconSize(formFactor)
    val passwordToggleSp = ScreenSpec.statusBarPasswordToggleSp(formFactor).sp

    val hostWifiLabel = stringResource(R.string.statusbar_host_wifi_name)
    val wifiPasswordLabel = stringResource(R.string.statusbar_wifi_password)
    val bluetoothLabel = stringResource(R.string.statusbar_bluetooth_name)
    val noOnlineDevicesText = stringResource(R.string.statusbar_no_online_devices)
    val noLogsText = stringResource(R.string.statusbar_no_logs)
    val clearLogsText = stringResource(R.string.action_clear)

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        WifiInfoRow(
            wifiName = wifiName,
            wifiPassword = wifiPassword,
            bluetoothName = bluetoothName,
            hostWifiLabel = hostWifiLabel,
            wifiPasswordLabel = wifiPasswordLabel,
            bluetoothLabel = bluetoothLabel,
            labelFontSize = infoLabelSp,
            valueFontSize = infoValueSp,
            passwordToggleFontSize = passwordToggleSp,
            modifier = Modifier.fillMaxWidth(),
        )
        ConnectedDevicesRow(
            devices = connectedDevices,
            nameFontSize = deviceNameSp,
            ipFontSize = deviceIpSp,
            placeholderFontSize = placeholderSp,
            emptyText = noOnlineDevicesText,
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.22f),
        )
        AlertLogsSection(
            logs = alertLogs,
            timeFontSize = logTimeSp,
            messageFontSize = logMessageSp,
            placeholderFontSize = placeholderSp,
            logIconSize = logIconSize,
            emptyText = noLogsText,
            clearText = clearLogsText,
            onClearClick = onClearAlertLogs,
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.58f),
        )
    }
}

/** 第一行：主机 WiFi / 密码 / 蓝牙，横向居中 */
@Composable
private fun WifiInfoRow(
    wifiName: String,
    wifiPassword: String,
    bluetoothName: String,
    hostWifiLabel: String,
    wifiPasswordLabel: String,
    bluetoothLabel: String,
    labelFontSize: TextUnit,
    valueFontSize: TextUnit,
    passwordToggleFontSize: TextUnit,
    modifier: Modifier = Modifier,
) {
    var passwordVisible by remember { mutableStateOf(false) }

    Row(
        modifier = modifier.padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        WifiInfoItem(
            label = hostWifiLabel,
            value = wifiName,
            labelFontSize = labelFontSize,
            valueFontSize = valueFontSize,
        )
        WifiInfoItem(
            label = wifiPasswordLabel,
            value = if (passwordVisible) wifiPassword else "********",
            labelFontSize = labelFontSize,
            valueFontSize = valueFontSize,
            trailing = {
                Text(
                    text = if (passwordVisible) "🙈" else "👁",
                    fontSize = passwordToggleFontSize,
                    modifier = Modifier
                        .padding(start = 6.dp)
                        .clickable { passwordVisible = !passwordVisible }
                        .padding(4.dp),
                )
            },
            modifier = Modifier.padding(horizontal = 20.dp),
        )
        WifiInfoItem(
            label = bluetoothLabel,
            value = bluetoothName,
            labelFontSize = labelFontSize,
            valueFontSize = valueFontSize,
        )
    }
}

@Composable
private fun WifiInfoItem(
    label: String,
    value: String,
    labelFontSize: TextUnit,
    valueFontSize: TextUnit,
    modifier: Modifier = Modifier,
    trailing: @Composable (() -> Unit)? = null,
) {
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "$label: ",
            color = RaylinkTextSecondary,
            fontSize = labelFontSize,
        )
        Text(
            text = value,
            color = RaylinkTextPrimary,
            fontSize = valueFontSize,
        )
        trailing?.invoke()
    }
}

/** 第二行：已连接设备，横向排列 */
@Composable
private fun ConnectedDevicesRow(
    devices: List<ConnectedDeviceUi>,
    nameFontSize: TextUnit,
    ipFontSize: TextUnit,
    placeholderFontSize: TextUnit,
    emptyText: String,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        if (devices.isEmpty()) {
            Box(
                modifier = Modifier.fillMaxSize(),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = emptyText,
                    color = RaylinkTextSecondary,
                    fontSize = placeholderFontSize,
                )
            }
        } else {
            LazyRow(
                modifier = Modifier.fillMaxSize(),
                horizontalArrangement = Arrangement.spacedBy(12.dp, Alignment.CenterHorizontally),
                verticalAlignment = Alignment.CenterVertically,
                contentPadding = PaddingValues(horizontal = 8.dp),
            ) {
                itemsIndexed(
                    items = devices,
                    key = { index, device -> "${index}_${device.name}_${device.ip}" },
                ) { _, device ->
                    ConnectedDeviceCard(
                        device = device,
                        nameFontSize = nameFontSize,
                        ipFontSize = ipFontSize,
                    )
                }
            }
        }
    }
}

@Composable
private fun ConnectedDeviceCard(
    device: ConnectedDeviceUi,
    nameFontSize: TextUnit,
    ipFontSize: TextUnit,
) {
    Column(
        modifier = Modifier
            .widthIn(min = 100.dp, max = 168.dp)
            .clip(RoundedCornerShape(10.dp))
            .background(
                if (device.isOnline) {
                    RaylinkTextPrimary.copy(alpha = 0.12f)
                } else {
                    RaylinkTextSecondary.copy(alpha = 0.15f)
                },
            )
            .padding(horizontal = 14.dp, vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            text = device.name,
            color = if (device.isOnline) RaylinkTextPrimary else RaylinkTextSecondary,
            fontSize = nameFontSize,
            textAlign = TextAlign.Center,
            maxLines = 1,
        )
        if (device.ip.isNotBlank()) {
            Text(
                text = device.ip,
                color = RaylinkTextSecondary,
                fontSize = ipFontSize,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(top = 4.dp),
                maxLines = 1,
            )
        }
    }
}

/** 第三行：系统日志，纵向滚动；右下角清空 */
@Composable
private fun AlertLogsSection(
    logs: List<SystemAlertLog>,
    timeFontSize: TextUnit,
    messageFontSize: TextUnit,
    placeholderFontSize: TextUnit,
    logIconSize: Dp,
    emptyText: String,
    clearText: String,
    onClearClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier) {
        if (logs.isEmpty()) {
            Text(
                text = emptyText,
                color = RaylinkTextSecondary,
                fontSize = placeholderFontSize,
                modifier = Modifier.align(Alignment.TopStart),
            )
        } else {
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(bottom = 28.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(logs, key = { it.id }) { log ->
                    AlertLogRow(
                        log = log,
                        timeFontSize = timeFontSize,
                        messageFontSize = messageFontSize,
                        iconSize = logIconSize,
                    )
                }
            }
        }
        Text(
            text = clearText,
            color = if (logs.isEmpty()) RaylinkTextSecondary.copy(alpha = 0.45f) else RaylinkTextPrimary,
            fontSize = 15.sp,
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .clickable(enabled = logs.isNotEmpty(), onClick = onClearClick)
                .padding(horizontal = 4.dp, vertical = 2.dp),
        )
    }
}

@Composable
private fun AlertLogRow(
    log: SystemAlertLog,
    timeFontSize: TextUnit,
    messageFontSize: TextUnit,
    iconSize: Dp,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            painter = painterResource(alertLogIconRes(log.kind)),
            contentDescription = null,
            tint = RaylinkTextPrimary,
            modifier = Modifier.size(iconSize),
        )
        Text(
            text = log.timeText,
            color = RaylinkTextSecondary,
            fontSize = timeFontSize,
            modifier = Modifier.padding(start = 10.dp),
        )
        Text(
            text = log.message,
            color = RaylinkTextPrimary,
            fontSize = messageFontSize,
            modifier = Modifier.padding(start = 12.dp),
        )
    }
}

private fun alertLogIconRes(kind: AlertLogKind): Int = when (kind) {
    AlertLogKind.Alarm -> R.drawable.ic_log_alarm
    AlertLogKind.Warning -> R.drawable.ic_log_warning
    AlertLogKind.Connected -> R.drawable.ic_log_connected
    AlertLogKind.PowerOff -> R.drawable.ic_log_power
    AlertLogKind.Rename -> R.drawable.ic_log_info
    AlertLogKind.Info -> R.drawable.ic_log_info
}
