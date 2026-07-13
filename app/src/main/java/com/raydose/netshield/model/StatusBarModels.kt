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

/** 下拉状态栏：系统警示/事件日志（最多 100 条持久化；[id] 供 LazyColumn 稳定 key） */
data class SystemAlertLog(
    val id: Long,
    val timeText: String,
    val message: String,
    val kind: AlertLogKind = AlertLogKind.Info,
    /** 事件发生时间；用于排序与持久化 */
    val timestampMillis: Long = 0L,
)
