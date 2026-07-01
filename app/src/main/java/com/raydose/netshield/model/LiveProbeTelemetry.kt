package com.raydose.netshield.model

/** TCP 0x23 实时上传 + 按需读配置（0x13）后的 UI 缓存 */
data class LiveProbeTelemetry(
    val isOnline: Boolean = false,
    val doseRateText: String = "---",
    val doseUnit: String = "μSv/h",
    val temperature: String = "---",
    val pressure: String = "---",
    val humidity: String = "---",
    val co2: String = "---",
    val pm25: String = "---",
    /** 辐射剂量上/下阈值报警（alarm_bit bit0/bit1）；不含其它环境或声光 bit */
    val hasAlarm: Boolean = false,
    val doorOpen: Boolean? = null,
    /** 0x23 第 7 项 alarm_bit */
    val alarmBit: Long? = null,
    val externalAlarmConnected: Boolean = false,
    /** reg 50/52 辐射上/下（×100 μSv/h） */
    val doseUpperUsv: String? = null,
    val doseLowerUsv: String? = null,
    val radiationUpperAlarmOn: Boolean? = null,
    val radiationLowerAlarmOn: Boolean? = null,
    /** reg 122 音量 0~100 */
    val volume: Float? = null,
    val slaveScreenOn: Boolean? = null,
    val alarmLightOn: Boolean? = null,
    val controlBit2Value: Long? = null,
)
