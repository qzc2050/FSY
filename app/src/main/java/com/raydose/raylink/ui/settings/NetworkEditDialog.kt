package com.raydose.raylink.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.raydose.raylink.R
import com.raydose.raylink.model.HostNetworkSettings
import com.raydose.raylink.model.SlaveNetworkCard
import com.raydose.raylink.model.isFactoryHostDisplayName
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.tr

internal val NetworkHostIdOptions = listOf(0x20, 0x40, 0x60)

sealed class NetworkEditTarget {
    data object Host : NetworkEditTarget()
    data class Slave(val index: Int) : NetworkEditTarget()
}

private val NetworkLabelSp = 20.sp
private val NetworkValueSp = 19.sp
private val NetworkLabelWidth = 220.dp

@Composable
fun NetworkEditDialog(
    host: HostNetworkSettings,
    slaves: List<SlaveNetworkCard>,
    initialTarget: NetworkEditTarget,
    onDismiss: () -> Unit,
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
) {
    val context = LocalContext.current
    val resolvedFetchSlaveWifi = onFetchSlaveWifi ?: { _, _, onDone ->
        onDone(false, context.tr(R.string.settings_slave_fetch_not_impl), null, null)
    }
    val hostTargetLabel = stringResource(R.string.network_target_host)
    val slaveFallbackLabel = stringResource(R.string.network_target_slave_fallback)
    val fetchLabel = stringResource(R.string.action_fetch)
    val fetchingLabel = stringResource(R.string.action_fetching)
    val hostGatewayHint = stringResource(R.string.network_host_gateway_hint)
    val slaveIdInvalidRange = stringResource(R.string.network_slave_id_invalid_range)

    val initialIndex = when (initialTarget) {
        NetworkEditTarget.Host -> 0
        is NetworkEditTarget.Slave -> initialTarget.index + 1
    }
    val defaultHostName = stringResource(R.string.host_default_display_name)
    var selectedIndex by remember { mutableIntStateOf(initialIndex.coerceIn(0, slaves.size)) }
    var draftHost by remember(host, defaultHostName) {
        mutableStateOf(
            if (isFactoryHostDisplayName(host.hostDisplayName)) {
                host.copy(hostDisplayName = defaultHostName)
            } else {
                host
            },
        )
    }
    var draftSlaves by remember { mutableStateOf(slaves) }
    var fetchingWifi by remember { mutableStateOf(false) }
    var fetchHint by remember { mutableStateOf<String?>(null) }

    val targetLabels = buildList {
        add(hostTargetLabel)
        addAll(draftSlaves.map { it.displayName.ifBlank { slaveFallbackLabel } })
    }
    val isHostSelected = selectedIndex == 0

    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.78f)
                .clip(RoundedCornerShape(12.dp))
                .background(com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel)
                .padding(24.dp),
        ) {
            Text(
                text = stringResource(R.string.network_edit_title),
                color = RaylinkTextPrimary,
                fontSize = 24.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 480.dp)
                    .verticalScroll(rememberScrollState())
                    .padding(top = 16.dp),
                verticalArrangement = Arrangement.spacedBy(SettingsFormRowSpacing),
            ) {
                NetworkDialogLabeledRow(label = stringResource(R.string.network_edit_target)) {
                    SettingsDropdownControl(
                        value = targetLabels.getOrElse(selectedIndex) { hostTargetLabel },
                        options = targetLabels,
                        onSelected = { selectedIndex = it },
                        valueFontSize = NetworkValueSp,
                        menuFontSize = 18.sp,
                        fillWidth = true,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth(),
                    )
                }

                if (isHostSelected) {
                    val deviceIdLabel = "0x${draftHost.hostDeviceId.toString(16).uppercase()}"
                    val deviceIdOptions = NetworkHostIdOptions.map { "0x${it.toString(16).uppercase()}" }
                    NetworkDialogLabeledRow(label = stringResource(R.string.network_label_device_id)) {
                        SettingsDropdownControl(
                            value = deviceIdLabel,
                            options = deviceIdOptions,
                            onSelected = { index ->
                                draftHost = draftHost.copy(hostDeviceId = NetworkHostIdOptions[index])
                            },
                            valueFontSize = NetworkValueSp,
                            menuFontSize = 18.sp,
                            fillWidth = true,
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxWidth(),
                        )
                    }
                    NetworkDialogTextRow(
                        label = stringResource(R.string.network_label_device_name),
                        value = draftHost.hostDisplayName,
                        onValueChange = { draftHost = draftHost.copy(hostDisplayName = it) },
                    )
                    NetworkDialogReadOnlyRow(
                        label = stringResource(R.string.network_label_ip),
                        value = draftHost.ipAddress.ifBlank { "—" },
                    )
                    NetworkDialogTextRow(
                        label = stringResource(R.string.network_label_wifi_name),
                        value = draftHost.wifiName,
                        onValueChange = { draftHost = draftHost.copy(wifiName = it) },
                    )
                    NetworkDialogTextRow(
                        label = stringResource(R.string.network_label_wifi_password),
                        value = draftHost.wifiPassword,
                        onValueChange = { draftHost = draftHost.copy(wifiPassword = it) },
                        isPassword = true,
                    )
                    NetworkDialogLabeledRow(label = stringResource(R.string.network_fetch_from_gateway)) {
                        SettingsInlineActionButton(
                            text = if (fetchingWifi) fetchingLabel else fetchLabel,
                            onClick = {
                                if (fetchingWifi) return@SettingsInlineActionButton
                                fetchingWifi = true
                                fetchHint = null
                                onFetchHostWifi { success, message, wifiName, wifiPassword ->
                                    fetchingWifi = false
                                    fetchHint = message
                                    if (success && !wifiName.isNullOrBlank()) {
                                        draftHost = draftHost.copy(
                                            wifiName = wifiName,
                                            wifiPassword = wifiPassword.orEmpty(),
                                        )
                                    }
                                }
                            },
                            filled = true,
                            modifier = Modifier.padding(end = 8.dp),
                        )
                        Text(
                            text = hostGatewayHint,
                            color = RaylinkTextSecondary,
                            fontSize = 15.sp,
                            modifier = Modifier.weight(1f),
                        )
                    }
                    if (fetchHint != null) {
                        Text(
                            text = fetchHint.orEmpty(),
                            color = RaylinkAccentBlue,
                            fontSize = 15.sp,
                            modifier = Modifier.padding(start = NetworkLabelWidth),
                        )
                    }
                    NetworkDialogTextRow(
                        label = stringResource(R.string.statusbar_bluetooth_name),
                        value = draftHost.bluetoothName,
                        onValueChange = { draftHost = draftHost.copy(bluetoothName = it) },
                    )
                } else {
                    val slaveIndex = selectedIndex - 1
                    val slave = draftSlaves.getOrNull(slaveIndex)
                    if (slave != null) {
                        val slaveDeviceId = slave.protoAddr.trim().toIntOrNull()
                        NetworkDialogReadOnlyRow(
                            label = stringResource(R.string.network_label_device_id),
                            value = slave.protoAddr,
                        )
                        NetworkDialogReadOnlyRow(
                            label = stringResource(R.string.network_label_ip),
                            value = slave.ip,
                        )
                        NetworkDialogTextRow(
                            label = stringResource(R.string.network_label_wifi_name),
                            value = slave.wifiName,
                            onValueChange = { value ->
                                draftSlaves = draftSlaves.toMutableList().also {
                                    it[slaveIndex] = slave.copy(wifiName = value)
                                }
                            },
                        )
                        NetworkDialogTextRow(
                            label = stringResource(R.string.network_label_wifi_password),
                            value = slave.wifiPassword,
                            onValueChange = { value ->
                                draftSlaves = draftSlaves.toMutableList().also {
                                    it[slaveIndex] = slave.copy(wifiPassword = value)
                                }
                            },
                            isPassword = true,
                        )
                        NetworkDialogLabeledRow(label = stringResource(R.string.network_fetch_from_gateway)) {
                            SettingsInlineActionButton(
                                text = if (fetchingWifi) fetchingLabel else fetchLabel,
                                onClick = {
                                    if (fetchingWifi) return@SettingsInlineActionButton
                                    if (slaveDeviceId == null || slaveDeviceId !in 1..253) {
                                        fetchHint = context.tr(
                                            R.string.network_slave_id_invalid,
                                            slave.protoAddr,
                                        )
                                        return@SettingsInlineActionButton
                                    }
                                    fetchingWifi = true
                                    fetchHint = null
                                    resolvedFetchSlaveWifi(slaveDeviceId, slave.ip) {
                                            success, message, wifiName, wifiPassword ->
                                        fetchingWifi = false
                                        fetchHint = message
                                        if (success && !wifiName.isNullOrBlank()) {
                                            draftSlaves = draftSlaves.toMutableList().also { list ->
                                                val current = list.getOrNull(slaveIndex) ?: return@also
                                                list[slaveIndex] = current.copy(
                                                    wifiName = wifiName,
                                                    wifiPassword = wifiPassword.orEmpty(),
                                                )
                                            }
                                        }
                                    }
                                },
                                filled = true,
                                modifier = Modifier.padding(end = 8.dp),
                            )
                            Text(
                                text = if (slaveDeviceId != null && slaveDeviceId in 1..253) {
                                    stringResource(
                                        R.string.network_slave_id_gateway_hint,
                                        slaveDeviceId,
                                        slaveDeviceId + 1,
                                    )
                                } else {
                                    slaveIdInvalidRange
                                },
                                color = RaylinkTextSecondary,
                                fontSize = 15.sp,
                                modifier = Modifier.weight(1f),
                            )
                        }
                        if (fetchHint != null) {
                            Text(
                                text = fetchHint.orEmpty(),
                                color = RaylinkAccentBlue,
                                fontSize = 15.sp,
                                modifier = Modifier.padding(start = NetworkLabelWidth),
                            )
                        }
                    }
                }
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 20.dp),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                SettingsInlineActionButton(
                    text = stringResource(R.string.action_cancel),
                    onClick = onDismiss,
                )
                SettingsInlineActionButton(
                    text = stringResource(R.string.action_save),
                    onClick = {
                        if (isHostSelected) {
                            onSaveHost(draftHost)
                        } else {
                            val slaveIndex = selectedIndex - 1
                            draftSlaves.getOrNull(slaveIndex)?.let { onSaveSlave(slaveIndex, it) }
                        }
                        onDismiss()
                    },
                    filled = true,
                    modifier = Modifier.padding(start = 12.dp),
                )
            }
        }
    }
}

@Composable
private fun NetworkDialogLabeledRow(
    label: String,
    content: @Composable RowScope.() -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = NetworkLabelWidth,
            fontSize = NetworkLabelSp,
            maxLines = 1,
            softWrap = false,
        )
        content()
    }
}

@Composable
private fun NetworkDialogTextRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    isPassword: Boolean = false,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = NetworkLabelWidth,
            fontSize = NetworkLabelSp,
            maxLines = 1,
            softWrap = false,
        )
        SettingsValueField(
            value = value,
            onValueChange = onValueChange,
            isPassword = isPassword,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun NetworkDialogReadOnlyRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = NetworkLabelWidth,
            fontSize = NetworkLabelSp,
            maxLines = 1,
            softWrap = false,
        )
        Text(
            text = value.ifBlank { "—" },
            color = RaylinkTextSecondary,
            fontSize = NetworkValueSp,
            modifier = Modifier.weight(1f),
        )
    }
}
