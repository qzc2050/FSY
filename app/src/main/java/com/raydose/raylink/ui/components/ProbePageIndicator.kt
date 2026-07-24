package com.raydose.raylink.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.raydose.raylink.ui.home.shouldEnableProbeAutoScroll
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.ScreenSpec
import kotlinx.coroutines.delay

/** 多探头自动翻页；[intervalMs] 默认主页 5s，待机页可传 2s。 */
@Composable
fun ProbePagerAutoScroll(
    enabled: Boolean,
    autoScrollPageIndices: List<Int>,
    pageCount: Int,
    pagerState: PagerState,
    paused: Boolean = false,
    intervalMs: Long = ScreenSpec.PROBE_CARD_AUTO_SCROLL_INTERVAL_MS,
) {
    val scrollPagesKey = autoScrollPageIndices.joinToString(",")
    LaunchedEffect(enabled, scrollPagesKey, pageCount, paused, intervalMs) {
        if (!enabled || paused) return@LaunchedEffect
        if (!shouldEnableProbeAutoScroll(autoScrollPageIndices, pageCount)) return@LaunchedEffect
        while (true) {
            delay(intervalMs)
            val current = pagerState.currentPage
            val nextPage = if (current in autoScrollPageIndices) {
                val idx = autoScrollPageIndices.indexOf(current)
                autoScrollPageIndices[(idx + 1) % autoScrollPageIndices.size]
            } else {
                autoScrollPageIndices.first()
            }
            if (nextPage != current) {
                pagerState.animateScrollToPage(nextPage)
            }
        }
    }
}

@Composable
fun ProbePageIndicator(
    pageCount: Int,
    currentPage: Int,
    modifier: Modifier = Modifier,
    dotSize: Dp = 8.dp,
    activeDotSize: Dp = 10.dp,
    dotSpacing: Dp = 10.dp,
) {
    if (pageCount <= 0) return

    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center,
    ) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(dotSpacing, Alignment.CenterHorizontally),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            repeat(pageCount) { index ->
                val selected = index == currentPage
                Box(
                    modifier = Modifier
                        .size(if (selected) activeDotSize else dotSize)
                        .clip(CircleShape)
                        .background(
                            if (selected) {
                                RaylinkTextPrimary
                            } else {
                                RaylinkTextSecondary.copy(alpha = 0.45f)
                            },
                        ),
                )
            }
        }
    }
}
