package com.raydose.netshield.ui.components

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
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import kotlinx.coroutines.delay

/** 多探头 + 滚动模式：每 [ScreenSpec.PROBE_CARD_AUTO_SCROLL_INTERVAL_MS] 自动翻页。 */
@Composable
fun ProbePagerAutoScroll(
    enabled: Boolean,
    probeCount: Int,
    pagerState: PagerState,
    paused: Boolean = false,
) {
    LaunchedEffect(enabled, probeCount, paused) {
        if (!enabled || paused || probeCount <= 1) return@LaunchedEffect
        while (true) {
            delay(ScreenSpec.PROBE_CARD_AUTO_SCROLL_INTERVAL_MS)
            pagerState.animateScrollToPage((pagerState.currentPage + 1) % probeCount)
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
                                NetShieldTextPrimary
                            } else {
                                NetShieldTextSecondary.copy(alpha = 0.45f)
                            },
                        ),
                )
            }
        }
    }
}
