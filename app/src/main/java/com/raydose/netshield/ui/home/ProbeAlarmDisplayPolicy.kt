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
 *
 * 正常滚动（无报警）时优先在线探头：
 * - 列表仍含全部探头（离线可手动滑到）
 * - 自动翻页仅在「含在线探头」的页之间循环
 * - 在线 ≥ 2：只滚在线页；在线 = 1 且有离线独占页：定时跳回在线页；全离线：滚全部页
 */
data class ProbePagerConfig(
    val displayProbes: List<SlaveProbeUi>,
    val probesPerPage: Int,
    val autoScroll: Boolean,
    val pageCount: Int,
    /** 参与自动翻页的页下标（离线独占页不在此列） */
    val autoScrollPageIndices: List<Int>,
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
                autoScrollPageIndices = emptyList(),
                alarmPriorityActive = true,
            )
        } else {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = true,
                pageCount = alarming.size,
                autoScrollPageIndices = (0 until alarming.size).toList(),
                alarmPriorityActive = true,
            )
        }
    }
    val perPage = visibleProbeCards.coerceIn(1, 4)
    val pages = probeGridPages(allProbes, perPage)
    val scrollPages = resolveOnlineFirstAutoScrollPageIndices(pages)
    val pageCount = pages.size.coerceAtLeast(1)
    val autoScroll = probeCardMode == ProbeCardDisplayMode.Scroll &&
        shouldEnableProbeAutoScroll(scrollPages, pageCount)
    return ProbePagerConfig(
        displayProbes = allProbes,
        probesPerPage = perPage,
        autoScroll = autoScroll,
        pageCount = pageCount,
        autoScrollPageIndices = if (autoScroll) scrollPages else emptyList(),
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
                autoScrollPageIndices = emptyList(),
                alarmPriorityActive = true,
            )
        } else {
            ProbePagerConfig(
                displayProbes = alarming,
                probesPerPage = 1,
                autoScroll = true,
                pageCount = alarming.size,
                autoScrollPageIndices = (0 until alarming.size).toList(),
                alarmPriorityActive = true,
            )
        }
    }
    val pages = probeGridPages(allProbes, probesPerPage = 1)
    val scrollPages = resolveOnlineFirstAutoScrollPageIndices(pages)
    val pageCount = pages.size.coerceAtLeast(1)
    val autoScroll = shouldEnableProbeAutoScroll(scrollPages, pageCount)
    return ProbePagerConfig(
        displayProbes = allProbes,
        probesPerPage = 1,
        autoScroll = autoScroll,
        pageCount = pageCount,
        autoScrollPageIndices = if (autoScroll) scrollPages else emptyList(),
        alarmPriorityActive = false,
    )
}

/** 按页切分探头格位（含 null 空位）。 */
fun probeGridPages(
    probes: List<SlaveProbeUi>,
    probesPerPage: Int,
): List<List<SlaveProbeUi?>> {
    val per = probesPerPage.coerceIn(1, 4)
    val pageCount = homeProbePageCount(probes.size, per)
    return (0 until pageCount).map { page ->
        homeProbeGridSlots(probes, page, per)
    }
}

/** 该页是否至少有一个在线探头。 */
fun probePageHasOnline(slots: List<SlaveProbeUi?>): Boolean =
    slots.any { it?.isOnline == true }

/**
 * 在线优先自动翻页页集合：
 * - 在线 ≥ 2 → 仅含在线探头的页
 * - 在线 = 1 → 仅含该在线探头的页（用于跳回在线；不在离线页之间轮播）
 * - 全离线 → 全部页
 */
fun resolveOnlineFirstAutoScrollPageIndices(
    pages: List<List<SlaveProbeUi?>>,
): List<Int> {
    if (pages.isEmpty()) return emptyList()
    val onlineCount = pages.flatten().filterNotNull().count { it.isOnline }
    return when {
        onlineCount >= 1 -> pages.indices.filter { probePageHasOnline(pages[it]) }
        else -> pages.indices.toList()
    }
}

/** 是否启用自动翻页/跳回：多在线页轮播，或仅 1 在线但有离线独占页需定时跳回。 */
fun shouldEnableProbeAutoScroll(scrollPages: List<Int>, pageCount: Int): Boolean {
    if (scrollPages.isEmpty() || pageCount <= 1) return false
    if (scrollPages.size > 1) return true
    return scrollPages.size < pageCount
}

/** 报警 / 在线状态变化时用于重置 Pager 的稳定 key */
fun probeDisplayListKey(probes: List<SlaveProbeUi>): String =
    probes.joinToString(separator = ",") { "${it.id}:${it.hasAlarm}:${it.isOnline}" }
