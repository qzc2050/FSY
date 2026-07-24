package com.raydose.raylink.model

data class HostEnvReading(
    val label: String,
    val value: String,
)

data class SlaveProbeUi(
    val id: String,
    val name: String,
    /** 从机 IP（组播/TCP 发现后填入，用于状态栏「已连接设备」） */
    val ip: String = "",
    val location: String = "",
    val isOnline: Boolean = false,
    val doseRateText: String = "---",
    val doseUnit: String = "μSv/h",
    val temperature: String = "---",
    val pressure: String = "---",
    val humidity: String = "---",
    val co2: String = "---",
    val pm25: String = "---",
    val hasAlarm: Boolean = false,
)

data class MessageItem(
    val id: Long,
    val text: String,
)

enum class DoorState {
    Open,
    Closed,
    Unknown,
}

data class HomeUiState(
    val systemName: String = "Raylink",
    val dateText: String = "",
    val timeText: String = "",
    /** 与设置 · 网络信息 · 主机区联动，供下拉面板第一行显示 */
    val hostNetwork: HostNetworkSettings = HostNetworkSettings(),
    val hostEnvReadings: List<HostEnvReading> = emptyList(),
    val hostEnvIndex: Int = 0,
    val slaveProbes: List<SlaveProbeUi> = emptyList(),
    val selectedProbeIndex: Int = 0,
    /**
     * 下拉第二行「在线设备」：已入库且在线的探头 + 未入库但组播/串口在线的本公司设备。
     * 与 [slaveProbes]（仅已添加探头）不同。
     */
    val statusBarDevices: List<ConnectedDeviceUi> = emptyList(),
    val doorState: DoorState = DoorState.Unknown,
    val latestAlert: String = "",
    /** 下拉状态栏第三列：系统日志（最多 100 条，本地持久化） */
    val alertLogs: List<SystemAlertLog> = emptyList(),
    val messages: List<MessageItem> = emptyList(),
    val statusBarExpanded: Boolean = false,
    val sideDrawerOpen: Boolean = false,
    /** 转接板 QCC3084 USB 蓝牙音频在线 */
    val bluetoothOnline: Boolean = false,
    /** 本机 eth* 有线网卡已有 IPv4 */
    val ethernetOnline: Boolean = false,
)
