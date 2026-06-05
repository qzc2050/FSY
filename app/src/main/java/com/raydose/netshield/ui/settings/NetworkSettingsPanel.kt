package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private val HostIdOptions = listOf(0x20, 0x40, 0x60)

@Composable
fun NetworkSettingsPanel(
    host: HostNetworkSettings,
    slaves: List<SlaveNetworkCard>,
    onHostChange: (HostNetworkSettings) -> Unit,
    onSlaveChange: (Int, SlaveNetworkCard) -> Unit,
    onSaveHost: () -> Unit,
    onSaveSlave: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        SettingsSectionTitle("主机")
        SettingsCard {
            SettingsDropdownHalfRow(
                label = "设备 ID",
                value = "0x${host.hostDeviceId.toString(16).uppercase()}",
                options = HostIdOptions.map { "0x${it.toString(16).uppercase()}" },
                onSelected = { index -> onHostChange(host.copy(hostDeviceId = HostIdOptions[index])) },
                alignSaveColumn = true,
            )

            SettingsTextFieldHalfRow(
                label = "设备名称",
                value = host.hostDisplayName,
                onValueChange = { onHostChange(host.copy(hostDisplayName = it)) },
                alignSaveColumn = true,
            )
            SettingsReadOnlyHalfRow(
                label = "IP 地址",
                value = host.ipAddress,
                alignSaveColumn = true,
            )
            SettingsTextFieldHalfRow(
                label = "WiFi 名称",
                value = host.wifiName,
                onValueChange = { onHostChange(host.copy(wifiName = it)) },
                alignSaveColumn = true,
            )
            SettingsTextFieldHalfRow(
                label = "WiFi 密码",
                value = host.wifiPassword,
                onValueChange = { onHostChange(host.copy(wifiPassword = it)) },
                alignSaveColumn = true,
                isPassword = true,
            )
            SettingsTextFieldHalfRowWithSave(
                label = "蓝牙名称",
                value = host.bluetoothName,
                onValueChange = { onHostChange(host.copy(bluetoothName = it)) },
                onSaveClick = onSaveHost,
                alignSaveColumn = true,
            )
        }

        SettingsSectionTitle("从机探头")
        if (slaves.isEmpty()) {
            Text(
                text = "暂无已保存探头，请先在「探头管理」中添加。",
                color = NetShieldTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(vertical = 12.dp),
            )
        } else {
            slaves.forEachIndexed { index, card ->
                SettingsCard(modifier = Modifier.padding(bottom = 8.dp)) {
                    Text(
                        text = card.displayName,
                        color = NetShieldTextPrimary,
                        fontSize = 18.sp,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                    SettingsReadOnlyHalfRow(
                        label = "设备 ID",
                        value = card.protoAddr,
                        alignSaveColumn = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = "IP 地址",
                        value = card.ip,
                        alignSaveColumn = true,
                    )
                    SettingsTextFieldHalfRow(
                        label = "WiFi 名称",
                        value = card.wifiName,
                        onValueChange = { onSlaveChange(index, card.copy(wifiName = it)) },
                        alignSaveColumn = true,
                    )
                    SettingsTextFieldHalfRowWithSave(
                        label = "WiFi 密码",
                        value = card.wifiPassword,
                        onValueChange = { onSlaveChange(index, card.copy(wifiPassword = it)) },
                        onSaveClick = { onSaveSlave(index) },
                        alignSaveColumn = true,
                        isPassword = true,
                    )
                }
            }
        }
    }
}
