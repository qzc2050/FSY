package com.raydose.raylink.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.model.HostNetworkSettings
import com.raydose.raylink.model.SlaveNetworkCard
import com.raydose.raylink.ui.displayNameText
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.tr

private val NetworkLabelSp = 20.sp
private val NetworkValueSp = 19.sp
private val NetworkLabelWidth = 220.dp

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
    onFetchSlaveWifi: ((
        deviceId: Int,
        slaveIp: String,
        onDone: (success: Boolean, message: String, wifiName: String?, wifiPassword: String?) -> Unit,
    ) -> Unit)? = null,
    modifier: Modifier = Modifier,
) {
    var showEditDialog by remember { mutableStateOf(false) }
    val context = LocalContext.current
    val resolvedFetchSlaveWifi = onFetchSlaveWifi ?: { _, _, onDone ->
        onDone(false, context.tr(R.string.settings_slave_fetch_not_impl), null, null)
    }

    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        SettingsSectionHeaderRow(
            title = stringResource(R.string.network_section_host),
            titleFontSize = NetworkLabelSp,
            onActionClick = { showEditDialog = true },
        )
        SettingsCard {
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.network_label_device_id),
                value = "0x${host.hostDeviceId.toString(16).uppercase()}",
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.network_label_device_name),
                value = host.displayNameText(),
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.network_label_ip),
                value = host.ipAddress,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.network_label_wifi_name),
                value = host.wifiName,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.network_label_wifi_password),
                value = maskPassword(host.wifiPassword),
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
            SettingsReadOnlyHalfRow(
                label = stringResource(R.string.statusbar_bluetooth_name),
                value = host.bluetoothName,
                labelFontSize = NetworkLabelSp,
                valueFontSize = NetworkValueSp,
                labelWidth = NetworkLabelWidth,
                labelSingleLine = true,
            )
        }

        SettingsSectionTitle(stringResource(R.string.network_section_slaves))
        if (slaves.isEmpty()) {
            Text(
                text = stringResource(R.string.network_no_saved_probes),
                color = RaylinkTextSecondary,
                fontSize = NetworkValueSp,
                modifier = Modifier.padding(vertical = 12.dp),
            )
        } else {
            slaves.forEach { card ->
                SettingsCard(modifier = Modifier.padding(bottom = 8.dp)) {
                    Text(
                        text = card.displayName,
                        color = RaylinkTextPrimary,
                        fontSize = 19.sp,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                    SettingsReadOnlyHalfRow(
                        label = stringResource(R.string.network_label_device_id),
                        value = card.protoAddr,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = stringResource(R.string.network_label_ip),
                        value = card.ip,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = stringResource(R.string.network_label_wifi_name),
                        value = card.wifiName,
                        labelFontSize = NetworkLabelSp,
                        valueFontSize = NetworkValueSp,
                        labelWidth = NetworkLabelWidth,
                        labelSingleLine = true,
                    )
                    SettingsReadOnlyHalfRow(
                        label = stringResource(R.string.network_label_wifi_password),
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
            onFetchSlaveWifi = resolvedFetchSlaveWifi,
        )
    }
}
