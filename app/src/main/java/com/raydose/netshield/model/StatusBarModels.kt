package com.raydose.netshield.model

/** 下拉状态栏：已连接设备（协议/组播发现后填入 IP 与名称） */
data class ConnectedDeviceUi(
    val name: String,
    val ip: String,
    val isOnline: Boolean = true,
)

enum class AlertLogKind {
    Alarm,
    Warning,
    Connected,
    Info,
    PowerOff,
    Rename,
}

/** 下拉状态栏：24h 系统警示/事件日志（[id] 供 LazyColumn 稳定 key，对接库表主键） */
data class SystemAlertLog(
    val id: Long,
    val timeText: String,
    val message: String,
    val kind: AlertLogKind = AlertLogKind.Info,
    /** 用于 24h 窗口过滤；旧数据可为 0 */
    val timestampMillis: Long = 0L,
)
