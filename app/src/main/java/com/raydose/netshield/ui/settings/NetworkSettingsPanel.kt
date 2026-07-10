package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private val NetworkLabelSp = 20.sp
private val NetworkValueSp = 19.sp
private val NetworkLabelWidth = 176.dp

private fun maskPassword(value: String): String =
    if (value.isBlank()) "—" else "•".repeat(value.length.coerceAtMost(12))

@Composable
fun NetworkSettingsPanel(
    host: HostNetworkSettings,
    slaves: List<SlaveNetworkCard>,
    onSaveHost: (HostNetworkSettings) -> Unit,
    onSaveSlave: (Int, SlaveNetworkCard) -> Unit,
    onFetchHostWifi: (
        (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit
    ) -> Unit = {},
    onFetchSlaveWifi: (
        deviceId: Int,
        slaveIp: String,
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) -> Unit = { _, _, onDone -> onDone(false, "未实现从机获取", null, null) },
    modifier: Modifier = Modifier,
) {
    var showEditDialog by remember { mutableStateOf(false) }

    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        SettingsSectionHeaderRow(
            title = "主机",
            titleFontSize = NetworkLabelSp,
            onActionClick = { showEditDialog = true },
        )
        SettingsCard {
            SettingsReadOnlyHalfRow(
                label = "设备 ID",
                value = "0x${host.hostDeviceId.toString(16).uppercase()}",
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = "设备名称",
                value = host.hostDisplayName,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = "IP 地址",
                value = host.ipAddress,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = "WiFi 名称",
                value = host.wifiName,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = "WiFi 密码",
                value = maskPassword(host.wifiPassword),
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = "蓝牙名称",
                value = host.bluetoothName,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
        }

        SettingsSectionTitle("从机探头")
        if (slaves.isEmpty()) {
            Text(
                text = "暂无已保存探头，请先在「探头管理」中添加。",
                color = NetShieldTextSecondary,
                fontSize = NetworkValueSp,
                modifier = Modifier.padding(vertical = 12.dp),
            )
        } else {
            slaves.forEach { card ->
                SettingsCard(modifier = Modifier.padding(bottom = 8.dp)) {
                    Text(
                        text = card.displayName,
                        color = NetShieldTextPrimary,
                        fontSize = 19.sp,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                    SettingsReadOnlyHalfRow(
                        label = "设备 ID",
                        value = card.protoAddr,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = "IP 地址",
                        value = card.ip,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = "WiFi 名称",
                        value = card.wifiName,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = "WiFi 密码",
                        value = maskPassword(card.wifiPassword),
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                }
            }
        }
    }

    if (showEditDialog) {
        NetworkEditDialog(
            host = host,
            slaves = slaves,
            initialTarget = NetworkEditTarget.Host,
            onDismiss = { showEditDialog = false },
            onSaveHost = onSaveHost,
            onSaveSlave = onSaveSlave,
            onFetchHostWifi = onFetchHostWifi,
            onFetchSlaveWifi = onFetchSlaveWifi,
        )
    }
}
