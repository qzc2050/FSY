package com.raydose.netshield.model

/** 设置 · 显示与声音 */
data class DisplaySoundSettings(
    val language: AppLanguage = AppLanguage.Zh,
    val probeCardMode: ProbeCardDisplayMode = ProbeCardDisplayMode.Fixed,
    val standbyMinutes: Int = 5,
    val brightness: Float = 0.7f,
    val systemVolume: Float = 0.7f,
    val hostAlarmVolume: Float = 0.7f,
    val promptVolume: Float = 0.7f,
    val visibleProbeCards: Int = 1,
    val mute: Boolean = false,
    val pauseAlarmFiveMinutes: Boolean = false,
)

enum class AppLanguage(val label: String) {
    Zh("中文"),
    En("English"),
}

enum class ProbeCardDisplayMode(val label: String) {
    Fixed("固定"),
    Scroll("滚动"),
}

/** 网络信息 · 主机 / 从机 WiFi 出厂默认值 */
object NetworkWifiDefaults {
    const val HOST_WIFI_NAME = "NST130_WIFI1"
    const val HOST_WIFI_PASSWORD = "raydose888"
    const val HOST_BLUETOOTH_NAME = "NST130_BLE1"
    const val SLAVE_WIFI_NAME = "NSD100_WIFI1"
    const val SLAVE_WIFI_PASSWORD = "raydose888"
}

/** 设置 · 网络信息（主机） */
data class HostNetworkSettings(
    val hostDeviceId: Int = 0x20,
    val hostDisplayName: String = "NetShield 主机",
    val ipAddress: String = "",
    val wifiName: String = NetworkWifiDefaults.HOST_WIFI_NAME,
    val wifiPassword: String = NetworkWifiDefaults.HOST_WIFI_PASSWORD,
    val bluetoothName: String = NetworkWifiDefaults.HOST_BLUETOOTH_NAME,
)

/** 从机网络展示/编辑（ID 与名称来自探头列表；从机无蓝牙名称） */
data class SlaveNetworkCard(
    val probeId: String,
    val displayName: String,
    val protoAddr: String,
    val ip: String,
    val wifiName: String = NetworkWifiDefaults.SLAVE_WIFI_NAME,
    val wifiPassword: String = NetworkWifiDefaults.SLAVE_WIFI_PASSWORD,
)

/** 设置 · 时间（仅主页显示样式；同步到设备使用本机当前时间） */
data class TimeSettings(
    val use24Hour: Boolean = true,
    val showLunar: Boolean = true,
    val showGregorian: Boolean = true,
    val showHoliday: Boolean = false,
)

/** 电子相册首版设置：图片选择与应用开关。 */
data class AlbumSettings(
    val selectedImageUri: String = "",
    val applyStandby: Boolean = false,
    val applyDesktop: Boolean = false,
    val applyMessageDesktop: Boolean = true,
)

data class AlbumMessage(
    val id: Long,
    val text: String,
    val createdAtMillis: Long,
)
