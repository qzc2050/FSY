package com.raydose.raylink.model

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
    /** 暂停本机报警截止时间（毫秒）；0 表示未暂停。 */
    val pauseAlarmUntilMillis: Long = 0L,
)

/** 清除已过期暂停；UI 开关状态 = [pauseAlarmUntilMillis] > now。 */
fun DisplaySoundSettings.withExpiredPauseCleared(
    nowMillis: Long = System.currentTimeMillis(),
): DisplaySoundSettings {
    if (pauseAlarmUntilMillis <= 0L || pauseAlarmUntilMillis > nowMillis) return this
    return copy(pauseAlarmUntilMillis = 0L)
}

fun DisplaySoundSettings.isPauseAlarmActive(nowMillis: Long = System.currentTimeMillis()): Boolean =
    pauseAlarmUntilMillis > nowMillis

fun DisplaySoundSettings.isHostAlarmSuppressed(nowMillis: Long = System.currentTimeMillis()): Boolean =
    mute || isPauseAlarmActive(nowMillis)

/** 暂停本机报警 5 分钟 */
const val PAUSE_ALARM_DURATION_MS = 5 * 60 * 1000L

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
    const val HOST_WIFI_NAME = "RKM13_WIFI1"
    const val HOST_WIFI_PASSWORD = "raydose888"
    const val HOST_BLUETOOTH_NAME = "RKM13_BLE1"
    const val SLAVE_WIFI_NAME = "RK100P_WIFI1"
    const val SLAVE_WIFI_PASSWORD = "raydose888"
}

/** 设置 · 网络信息（主机） */
data class HostNetworkSettings(
    val hostDeviceId: Int = 0x20,
    val hostDisplayName: String = "Raylink 主机",
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

/** 设置 · 时间（显示样式 + 自动同步到探头；手动/自动同步均使用本机当前时间） */
data class TimeSettings(
    val use24Hour: Boolean = true,
    val showLunar: Boolean = true,
    val showGregorian: Boolean = true,
    val showHoliday: Boolean = false,
    /** 自动向在线探头写入 reg94；默认开启，间隔见 [PROBE_AUTO_SYNC_INTERVAL_MS] */
    val autoSyncToProbe: Boolean = true,
)

/** 电子相册：图片与留言展示设置。 */
data class AlbumSettings(
    val selectedImageUri: String = "",
    /** 上次选图时浏览的存储位置 */
    val lastPickerStorage: FileStorageLocation = FileStorageLocation.Local,
    /** 上次选图时所在目录（默认优先 Pictures） */
    val lastPickerDirectory: String = "",
    /** 上次选中的源文件路径（导入前；U 盘拔出后仅作记录，展示用 selectedImageUri） */
    val lastSelectedSourcePath: String = "",
    /** 所选图片作为待机页全屏背景 */
    val applyStandby: Boolean = false,
    /** 主页底部留言栏是否显示 */
    val showHomeMessages: Boolean = true,
    /** 待机页右侧留言区是否显示 */
    val showStandbyMessages: Boolean = true,
)

data class AlbumMessage(
    val id: Long,
    val text: String,
    val createdAtMillis: Long,
)

/** 每探头自动时间同步状态（持久化） */
data class ProbeTimeSyncState(
    val lastAutoSyncMillis: Long = 0L,
    val lastOfflineMillis: Long = 0L,
)
