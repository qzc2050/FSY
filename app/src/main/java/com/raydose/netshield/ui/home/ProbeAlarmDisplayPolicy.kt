package com.raydose.netshield.ui.home

import com.raydose.netshield.model.ProbeCardDisplayMode
import com.raydose.netshield.model.SlaveProbeUi

/**
 * 主页 / 待机页探头翻页策略。
 *
 * 任一探头报警时优先展示报警探头：
 * - 仅 1 个报警 → 固定显示该卡，不滚动
 * - 多个报警 → 仅在报警探头间自动翻页（每次 1 张全卡）
 * - 无报警 → 按设置的正常显示与滚动规则
 */
data class ProbePagerConfig(
    val displayProbes: List<SlaveProbeUi>,
    val probesPerPage: Int,
    val autoScroll: Boolean,
    val pageCount: Int,
    /** 是否处于报警优先模式（用于指示点等） */
    val alarmPriorityActive: Boolean,
)

fun resolveHomeProbePagerConfig(
    allProbes: List<SlaveProbeUi>,
    visibleProbeCards: Int,
    probeCardMode: ProbeCardDisplayMode,
): ProbePagerConfig {
    val alarming = allProbes.filter { it.hasAlarm }
    if (alarming.isNotEmpty()) {
        return if (alarming.size == 1) {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = false,
                pageCount = 1,
                alarmPriorityActive = true,
            )
        } else {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = true,
                pageCount = alarming.size,
                alarmPriorityActive = true,
            )
        }
    }
    val perPage = visibleProbeCards.coerceIn(1, 4)
    val pages = homeProbePageCount(allProbes.size, perPage)
    return ProbePagerConfig(
        displayProbes = allProbes,
        probesPerPage = perPage,
        autoScroll = probeCardMode == ProbeCardDisplayMode.Scroll && pages > 1,
        pageCount = pages,
        alarmPriorityActive = false,
    )
}

fun resolveStandbyProbePagerConfig(allProbes: List<SlaveProbeUi>): ProbePagerConfig {
    val alarming = allProbes.filter { it.hasAlarm }
    if (alarming.isNotEmpty()) {
        return if (alarming.size == 1) {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = false,
                pageCount = 1,
                alarmPriorityActive = true,
            )
        } else {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = true,
                pageCount = alarming.size,
                alarmPriorityActive = true,
            )
        }
    }
    val count = allProbes.size
    return ProbePagerConfig(
        displayProbes = allProbes,
        probesPerPage = 1,
        autoScroll = count > 1,
        pageCount = count.coerceAtLeast(1),
        alarmPriorityActive = false,
    )
}

/** 报警集合变化时用于重置 Pager 的稳定 key */
fun probeDisplayListKey(probes: List<SlaveProbeUi>): String =
    probes.joinToString(separator = ",") { "${it.id}:${it.hasAlarm}" }
