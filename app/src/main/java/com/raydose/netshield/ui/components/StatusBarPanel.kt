package com.raydose.netshield.ui.components

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
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.R
import com.raydose.netshield.model.AlertLogKind
import com.raydose.netshield.model.ConnectedDeviceUi
import com.raydose.netshield.model.SystemAlertLog
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.rememberTabletFormFactor

/**
 * 下拉面板纵向三行（image11）：
 * ① 主机 WiFi/蓝牙（整行居中） ② 已连接设备（横向） ③ 日志（纵向滚动）
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
            label = "主机WIFI名称",
            value = wifiName,
            labelFontSize = labelFontSize,
            valueFontSize = valueFontSize,
        )
        WifiInfoItem(
            label = "WIFI密码",
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
            label = "蓝牙名称",
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
            color = NetShieldTextSecondary,
            fontSize = labelFontSize,
        )
        Text(
            text = value,
            color = NetShieldTextPrimary,
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
                    text = "暂无在线设备",
                    color = NetShieldTextSecondary,
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
                    NetShieldTextPrimary.copy(alpha = 0.12f)
                } else {
                    NetShieldTextSecondary.copy(alpha = 0.15f)
                },
            )
            .padding(horizontal = 14.dp, vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(
            text = device.name,
            color = if (device.isOnline) NetShieldTextPrimary else NetShieldTextSecondary,
            fontSize = nameFontSize,
            textAlign = TextAlign.Center,
            maxLines = 1,
        )
        if (device.ip.isNotBlank()) {
            Text(
                text = device.ip,
                color = NetShieldTextSecondary,
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
    onClearClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier) {
        if (logs.isEmpty()) {
            Text(
                text = "暂无日志",
                color = NetShieldTextSecondary,
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
            text = "清空",
            color = if (logs.isEmpty()) NetShieldTextSecondary.copy(alpha = 0.45f) else NetShieldTextPrimary,
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
            tint = NetShieldTextPrimary,
            modifier = Modifier.size(iconSize),
        )
        Text(
            text = log.timeText,
            color = NetShieldTextSecondary,
            fontSize = timeFontSize,
            modifier = Modifier.padding(start = 10.dp),
        )
        Text(
            text = log.message,
            color = NetShieldTextPrimary,
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
