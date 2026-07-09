package com.raydose.netshield.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.HostEnvReading
import com.raydose.netshield.model.HomeUiState
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.ConnectivityStatusIconsRow
import com.raydose.netshield.ui.components.DoorStatusChip
import com.raydose.netshield.ui.components.SlaveProbeCard
import com.raydose.netshield.ui.theme.NetShieldMessageBar
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.rememberTabletFormFactor

/**
 * 下拉展开时下半屏「主页缩略」：
 * - 第一行 10%：门 1/4 · 留言 1/2 · 图标 1/4（整体下移 5%）
 * - 第二行 85%：环境 1/3 · 探头 2/3；环境参数三行（温湿 / CO2·PM2.5 / 气压）
 * - 第三行 5%：底留白
 */
@Composable
fun HomeThumbnailContent(
    state: HomeUiState,
    displayProbes: List<SlaveProbeUi>,
    probesPerPage: Int,
    pagerState: PagerState,
    currentMessageLine: String,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .fillMaxWidth(),
    ) {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(ScreenSpec.STATUS_BAR_THUMBNAIL_ROW1_FRACTION)
                .padding(horizontal = ScreenSpec.homeHorizontalPadding),
        ) {
            val topInset = maxHeight * ScreenSpec.STATUS_BAR_THUMBNAIL_ROW_TOP_INSET_FRACTION
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(maxHeight - topInset)
                    .padding(top = topInset),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(ScreenSpec.STATUS_BAR_THUMBNAIL_DOOR_FRACTION),
                    contentAlignment = Alignment.Center,
                ) {
                    DoorStatusChip(doorState = state.doorState)
                }
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(ScreenSpec.STATUS_BAR_THUMBNAIL_MESSAGE_FRACTION)
                        .padding(horizontal = 8.dp),
                    contentAlignment = Alignment.Center,
                ) {
                    ThumbnailMessageLine(
                        text = currentMessageLine,
                        modifier = Modifier
                            .fillMaxWidth()
                            .fillMaxHeight(0.88f),
                    )
                }
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(ScreenSpec.STATUS_BAR_THUMBNAIL_ICONS_FRACTION),
                    contentAlignment = Alignment.Center,
                ) {
                    ConnectivityStatusIconsRow(
                        showPullIndicator = false,
                        iconSize = 20.dp,
                    )
                }
            }
        }

        BoxWithConstraints(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(ScreenSpec.STATUS_BAR_THUMBNAIL_ROW2_FRACTION)
                .padding(horizontal = ScreenSpec.homeHorizontalPadding),
        ) {
            val topInset = maxHeight * ScreenSpec.STATUS_BAR_THUMBNAIL_ROW2_TOP_INSET_FRACTION
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(maxHeight - topInset)
                    .padding(top = topInset),
                verticalAlignment = Alignment.Top,
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(ScreenSpec.STATUS_BAR_THUMBNAIL_ENV_FRACTION),
                    contentAlignment = Alignment.TopCenter,
                ) {
                    ThumbnailEnvColumn(
                        dateText = state.dateText,
                        timeText = state.timeText,
                        readings = state.hostEnvReadings,
                        modifier = Modifier
                            .fillMaxWidth()
                            .fillMaxHeight(),
                    )
                }
                Box(
                    modifier = Modifier
                        .fillMaxHeight()
                        .weight(ScreenSpec.STATUS_BAR_THUMBNAIL_PROBE_FRACTION)
                        .padding(start = 10.dp),
                    contentAlignment = Alignment.TopStart,
                ) {
                    ThumbnailProbeArea(
                        displayProbes = displayProbes,
                        probesPerPage = probesPerPage,
                        pagerState = pagerState,
                        modifier = Modifier
                            .fillMaxWidth()
                            .fillMaxHeight(ScreenSpec.STATUS_BAR_THUMBNAIL_PROBE_HEIGHT_FRACTION),
                    )
                }
            }
        }

        Spacer(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(ScreenSpec.STATUS_BAR_THUMBNAIL_ROW3_FRACTION),
        )
    }
}

@Composable
private fun ThumbnailProbeArea(
    displayProbes: List<SlaveProbeUi>,
    probesPerPage: Int,
    pagerState: PagerState,
    modifier: Modifier = Modifier,
) {
    if (displayProbes.isEmpty()) {
        SlaveProbeCard(
            probe = SlaveProbeUi(id = "0", name = "暂无探头", isOnline = false),
            onDetailClick = {},
            modifier = modifier,
        )
    } else {
        HorizontalPager(
            state = pagerState,
            modifier = modifier,
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

@Composable
private fun ThumbnailMessageLine(
    text: String,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(18.dp))
            .background(NetShieldMessageBar)
            .padding(horizontal = 14.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Text(
            text = text.ifBlank { "暂无留言" },
            color = NetShieldTextPrimary,
            fontSize = ScreenSpec.STATUS_BAR_THUMBNAIL_MESSAGE_SP.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}

@Composable
private fun ThumbnailEnvColumn(
    dateText: String,
    timeText: String,
    readings: List<HostEnvReading>,
    modifier: Modifier = Modifier,
) {
    val envWeight = ScreenSpec.STATUS_BAR_THUMBNAIL_PROBE_ENV_WEIGHT
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f - envWeight),
        ) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(
                    ScreenSpec.statusBarThumbnailEnvLineSpacing,
                    Alignment.Top,
                ),
            ) {
                Text(
                    text = dateText,
                color = NetShieldTextSecondary,
                fontSize = ScreenSpec.STATUS_BAR_THUMBNAIL_DATE_SP.sp,
                fontWeight = FontWeight.Light,
                lineHeight = (ScreenSpec.STATUS_BAR_THUMBNAIL_DATE_SP + 6).sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth(),
            )
            Text(
                text = timeText,
                color = NetShieldTextPrimary,
                fontSize = ScreenSpec.STATUS_BAR_THUMBNAIL_TIME_SP.sp,
                fontWeight = FontWeight.Light,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                textAlign = TextAlign.Center,
                modifier = Modifier.fillMaxWidth(),
            )
            }
            Spacer(modifier = Modifier.weight(1f))
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(
                    ScreenSpec.statusBarThumbnailEnvLineSpacing,
                    Alignment.Bottom,
                ),
            ) {
                ThumbnailEnvLine("温度 ${envValue(readings, "温度")}  湿度 ${envValue(readings, "湿度")}")
                ThumbnailEnvLine("CO2 ${envValue(readings, "CO2")}  PM2.5 ${envValue(readings, "PM2.5")}")
            }
        }
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .weight(envWeight),
            contentAlignment = Alignment.Center,
        ) {
            ThumbnailEnvLine("气压 ${envValue(readings, "气压")}")
        }
    }
}

@Composable
private fun ThumbnailEnvLine(
    text: String,
    modifier: Modifier = Modifier,
) {
    val envSp = ScreenSpec.statusBarThumbnailEnvSp(rememberTabletFormFactor())
    Text(
        text = text,
        color = NetShieldTextPrimary,
        fontSize = envSp.sp,
        lineHeight = (envSp + 4).sp,
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        textAlign = TextAlign.Center,
        modifier = modifier.fillMaxWidth(),
    )
}

private fun envValue(readings: List<HostEnvReading>, label: String): String =
    readings.firstOrNull { it.label == label }?.value ?: "---"
