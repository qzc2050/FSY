package com.raydose.netshield.model

/** 本地持久化的 5 分钟累计剂量采样点（μSv，非剂量率）。 */
data class ProbeDoseSample(
    val probeId: String,
    val doseUsv: Double,
    val recordedAtMillis: Long,
)

/** 按日汇总的累积剂量，供探头详情页列表展示。 */
data class DailyDoseSummary(
    val dateText: String,
    val accumDoseText: String,
)
