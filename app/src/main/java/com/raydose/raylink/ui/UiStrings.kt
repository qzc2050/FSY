package com.raydose.raylink.ui

import android.content.Context
import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.raydose.raylink.R
import com.raydose.raylink.model.AppLanguage
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.HostNetworkSettings
import com.raydose.raylink.model.MusicPlayMode
import com.raydose.raylink.model.NeijiEnvSensorKind
import com.raydose.raylink.model.ProbeCardDisplayMode
import com.raydose.raylink.model.ProbeCommandLink
import com.raydose.raylink.model.isFactoryHostDisplayName
import com.raydose.raylink.net.Hlk7688WifiClient
import com.raydose.raylink.ui.settings.SettingsTab

fun Context.tr(@StringRes id: Int, vararg args: Any): String =
    if (args.isEmpty()) getString(id) else getString(id, *args)

/**
 * 本机环境参数标签：数据层仍用中文作稳定 key（温度/湿度/气压），展示时再本地化。
 * CO2 / PM2.5 保持符号不变。
 */
@Composable
fun localizeHostEnvLabel(label: String): String = when (label) {
    "温度" -> stringResource(R.string.env_temperature)
    "湿度" -> stringResource(R.string.env_humidity)
    "气压" -> stringResource(R.string.env_pressure)
    else -> label
}

fun Context.localizeHostEnvLabel(label: String): String = when (label) {
    "温度" -> tr(R.string.env_temperature)
    "湿度" -> tr(R.string.env_humidity)
    "气压" -> tr(R.string.env_pressure)
    else -> label
}

@Composable
fun HostNetworkSettings.displayNameText(): String =
    if (isFactoryHostDisplayName(hostDisplayName)) {
        stringResource(R.string.host_default_display_name)
    } else {
        hostDisplayName
    }

fun Context.resolveHostDisplayName(stored: String): String =
    if (isFactoryHostDisplayName(stored)) tr(R.string.host_default_display_name) else stored.trim()

@Composable
fun AppLanguage.labelText(): String = stringResource(
    when (this) {
        AppLanguage.Zh -> R.string.lang_chinese
        AppLanguage.En -> R.string.lang_english
    },
)

@Composable
fun ProbeCardDisplayMode.labelText(): String = stringResource(
    when (this) {
        ProbeCardDisplayMode.Fixed -> R.string.probe_display_fixed
        ProbeCardDisplayMode.Scroll -> R.string.probe_display_scroll
    },
)

@Composable
fun SettingsTab.labelText(): String = stringResource(
    when (this) {
        SettingsTab.DisplaySound -> R.string.settings_tab_display_sound
        SettingsTab.Network -> R.string.settings_tab_network
        SettingsTab.Time -> R.string.settings_tab_time
        SettingsTab.Probes -> R.string.settings_tab_probes
        SettingsTab.About -> R.string.settings_tab_about
    },
)

@StringRes
fun FileStorageLocation.labelRes(): Int = when (this) {
    FileStorageLocation.Local -> R.string.storage_local
    FileStorageLocation.Usb -> R.string.storage_usb
}

fun FileStorageLocation.labelText(context: Context): String = context.tr(labelRes())

@Composable
fun FileStorageLocation.labelText(): String = stringResource(labelRes())

@StringRes
fun MusicPlayMode.labelRes(): Int = when (this) {
    MusicPlayMode.LIST_LOOP -> R.string.music_mode_list_loop
    MusicPlayMode.SINGLE_LOOP -> R.string.music_mode_single_loop
    MusicPlayMode.SHUFFLE -> R.string.music_mode_shuffle
}

fun MusicPlayMode.labelText(context: Context): String = context.tr(labelRes())

@Composable
fun MusicPlayMode.labelText(): String = stringResource(labelRes())

fun NeijiEnvSensorKind.labelText(context: Context): String = when (this) {
    NeijiEnvSensorKind.TEMP_HUMIDITY -> context.tr(R.string.sensor_temp_humidity)
    NeijiEnvSensorKind.PRESSURE -> context.tr(R.string.sensor_pressure)
    NeijiEnvSensorKind.CO2 -> label
    NeijiEnvSensorKind.PM25 -> label
}

fun Context.linkLabel(link: ProbeCommandLink?): String = when (link) {
    ProbeCommandLink.NETWORK -> tr(R.string.link_network)
    ProbeCommandLink.SERIAL -> tr(R.string.link_serial)
    null -> tr(R.string.link_none)
}

fun Context.localizeWifiFetchError(error: Hlk7688WifiClient.WifiFetchError): String = when (error) {
    is Hlk7688WifiClient.WifiFetchError.HostIpInvalid ->
        tr(R.string.wifi_err_host_ip_invalid, error.hostIp)
    is Hlk7688WifiClient.WifiFetchError.SlaveIdInvalid ->
        tr(R.string.wifi_err_slave_id_invalid, error.deviceId)
    is Hlk7688WifiClient.WifiFetchError.SubnetInvalid ->
        tr(R.string.wifi_err_subnet_invalid, error.subnetIp)
    is Hlk7688WifiClient.WifiFetchError.NoSsidParsed ->
        tr(R.string.wifi_err_no_ssid, error.gatewayIp)
    is Hlk7688WifiClient.WifiFetchError.EmptySsid ->
        tr(R.string.wifi_err_empty_ssid, error.gatewayIp)
    is Hlk7688WifiClient.WifiFetchError.ConnectFailed ->
        tr(R.string.wifi_err_connect_failed, error.gatewayIp)
    is Hlk7688WifiClient.WifiFetchError.ReadFailed ->
        tr(R.string.wifi_err_read_failed, error.detail)
}

/** Maps OTA client progress text (Chinese keys from firmware layer) to localized UI strings. */
fun Context.localizeOtaProgress(statusText: String): String {
    if (statusText == "发送 OTA_START") return tr(R.string.ota_sending_start)
    if (statusText == "发送 OTA_DONE") return tr(R.string.ota_sending_done)
    if (statusText.startsWith("发送固件 ")) {
        val parts = statusText.removePrefix("发送固件 ").split('/')
        if (parts.size == 2) {
            val index = parts[0].trim().toIntOrNull()
            val total = parts[1].trim().toIntOrNull()
            if (index != null && total != null) {
                return tr(R.string.ota_sending_chunk, index, total)
            }
        }
    }
    if (statusText == "固件已发送，探头将校验并重启") return tr(R.string.ota_probe_sent_reboot)
    if (statusText == "固件已发送，转接板将校验并重启") return tr(R.string.ota_zjb_sent_reboot)
    return statusText
}
