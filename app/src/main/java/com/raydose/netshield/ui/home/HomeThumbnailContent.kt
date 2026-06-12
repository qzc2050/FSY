package com.raydose.netshield.ui.home

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.raydose.netshield.model.HomeUiState
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.DateTimeColumn
import com.raydose.netshield.ui.components.HostEnvScrollPanel
import com.raydose.netshield.ui.components.SlaveProbeCard
import com.raydose.netshield.ui.theme.ScreenSpec

/**
 * 下拉展开时下半屏「主页缩略」：仅含系统时间、本机环境、探头卡片（不含门状态与留言板）。
 */
@Composable
fun HomeThumbnailContent(
    state: HomeUiState,
    displayProbes: List<SlaveProbeUi>,
    probesPerPage: Int,
    pagerState: PagerState,
    screenWidth: Dp,
    screenHeight: Dp,
    modifier: Modifier = Modifier,
) {
    val cardHeight = screenHeight * ScreenSpec.HOME_CARD_HEIGHT_FRACTION
    fun x(percent: Float) = screenWidth * percent

    Column(modifier = modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(ScreenSpec.homeHostEnvPanelHeight)
                .padding(horizontal = ScreenSpec.homeHorizontalPadding)
                .padding(bottom = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.Top,
        ) {
            DateTimeColumn(
                dateText = state.dateText,
                timeText = state.timeText,
            )
            HostEnvScrollPanel(
                readings = state.hostEnvReadings.map { it.label to it.value },
            )
        }

        Spacer(modifier = Modifier.height(ScreenSpec.homeCardTopGap))

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(cardHeight),
        ) {
            Box(
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .offset(x = x(ScreenSpec.HOME_CARD_START_FRACTION))
                    .width(x(ScreenSpec.HOME_CARD_WIDTH_FRACTION))
                    .fillMaxHeight(),
            ) {
                if (displayProbes.isEmpty()) {
                    SlaveProbeCard(
                        probe = SlaveProbeUi(id = "0", name = "暂无探头", isOnline = false),
                        onDetailClick = {},
                        modifier = Modifier.fillMaxSize(),
                    )
                } else {
                    HorizontalPager(
                        state = pagerState,
                        modifier = Modifier.fillMaxSize(),
                        userScrollEnabled = false,
                    ) { page ->
                        if (probesPerPage == 1) {
                            SlaveProbeCard(
                                probe = displayProbes[page],
                                onDetailClick = {},
                                modifier = Modifier.fillMaxSize(),
                            )
                        } else {
                            HomeProbeGridPage(
                                slots = homeProbeGridSlots(
                                    probes = displayProbes,
                                    page = page,
                                    visiblePerPage = probesPerPage,
                                ),
                                visiblePerPage = probesPerPage,
                                onProbeDetailClick = {},
                                modifier = Modifier.fillMaxSize(),
                            )
                        }
                    }
                }
            }
        }
    }
}
