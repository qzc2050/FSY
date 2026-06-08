package com.raydose.netshield.model

/** 本地持久化的 5 分钟剂量采样点（后续由 0x23 实时数据写入）。 */
data class ProbeDoseSample(
    val probeId: String,
    val doseRateUsvH: Double,
    val recordedAtMillis: Long,
)

/** 按日汇总的累积剂量，供探头详情页列表展示。 */
data class DailyDoseSummary(
    val dateText: String,
    val accumDoseText: String,
)
