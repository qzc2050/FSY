package com.raydose.raylink.ui.standby

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.ui.localizeHostEnvLabel
import com.raydose.raylink.model.AlbumMessage
import com.raydose.raylink.model.AlbumSettings
import com.raydose.raylink.model.HomeUiState
import com.raydose.raylink.model.SlaveProbeUi
import com.raydose.raylink.model.TimeSettings
import com.raydose.raylink.ui.components.DoorStatusChip
import com.raydose.raylink.ui.components.HomeTopBar
import com.raydose.raylink.ui.components.ProbePagerAutoScroll
import com.raydose.raylink.ui.components.ProbePageIndicator
import com.raydose.raylink.ui.components.SlaveProbeCard
import com.raydose.raylink.ui.home.HomeClockFormatter
import com.raydose.raylink.ui.home.HomePreviewData
import com.raydose.raylink.ui.home.probeDisplayListKey
import com.raydose.raylink.ui.home.resolveStandbyProbePagerConfig
import com.raydose.raylink.ui.theme.RaylinkStandbyMessageBg
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.RaylinkTheme
import com.raydose.raylink.ui.theme.ScreenSpec
import com.raydose.raylink.ui.theme.TabletFormFactor
import com.raydose.raylink.ui.theme.rememberTabletFormFactor
import java.util.Date

@Composable
fun StandbyScreen(
    state: HomeUiState,
    albumSettings: AlbumSettings,
    messages: List<AlbumMessage>,
    timeSettings: TimeSettings,
    onExit: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val probePagerKey = probeDisplayListKey(state.slaveProbes)
    val pagerConfig = remember(probePagerKey) {
        resolveStandbyProbePagerConfig(state.slaveProbes)
    }
    val displayProbes = if (pagerConfig.alarmPriorityActive) {
        state.slaveProbes.filter { it.hasAlarm }
    } else {
        state.slaveProbes
    }
    val autoScroll = pagerConfig.autoScroll
    val displayListKey = probePagerKey
    val pagerState = rememberPagerState(pageCount = { pagerConfig.pageCount.coerceAtLeast(1) })
    val showStandbyMessages = albumSettings.showStandbyMessages
    val formFactor = rememberTabletFormFactor()
    val envReadings = state.hostEnvReadings.map {
        localizeHostEnvLabel(it.label) to it.value
    }
    val noProbeText = stringResource(R.string.home_no_probe)
    val noMessagesText = stringResource(R.string.home_no_messages)
    val resources = LocalContext.current.resources
    val latestMessages = remember(messages) {
        messages.sortedByDescending { it.createdAtMillis }.take(3)
    }
    val clockLines = remember(state.dateText, state.timeText, timeSettings, resources) {
        HomeClockFormatter.formatStandbyLines(
            now = Date(),
            prefs = timeSettings,
            resources = resources,
            fallbackDateText = state.dateText,
            fallbackTimeText = state.timeText,
        )
    }

    LaunchedEffect(displayListKey) {
        if (pagerState.currentPage != 0) {
            pagerState.scrollToPage(0)
        }
    }

    ProbePagerAutoScroll(
        enabled = autoScroll,
        autoScrollPageIndices = pagerConfig.autoScrollPageIndices,
        pageCount = pagerConfig.pageCount,
        pagerState = pagerState,
        intervalMs = ScreenSpec.STANDBY_PROBE_AUTO_SCROLL_INTERVAL_MS,
    )

    Box(
        modifier = modifier
            .fillMaxSize()
            .pointerInput(onExit) {
                detectTapGestures(onDoubleTap = { onExit() })
            },
    ) {
        StandbyBackground(albumSettings = albumSettings)

        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val screenHeight = maxHeight

            Column(modifier = Modifier.fillMaxSize()) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .fillMaxHeight(ScreenSpec.standbyHeaderSectionFraction(formFactor)),
                ) {
                    HomeTopBar(
                        systemName = state.systemName,
                        bluetoothOnline = state.bluetoothOnline,
                        ethernetOnline = state.ethernetOnline,
                    )
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f)
                            .padding(horizontal = ScreenSpec.homeHorizontalPadding),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center,
                    ) {
                        Text(
                            text = clockLines.dateLine.ifBlank { "—" },
                            color = RaylinkTextSecondary,
                            fontSize = ScreenSpec.standbyDateSp(formFactor).sp,
                            fontWeight = FontWeight.Light,
                            textAlign = TextAlign.Center,
                        )
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(top = 6.dp),
                        ) {
                            Text(
                                text = clockLines.timeLine.ifBlank { "—" },
                                color = RaylinkTextPrimary,
                                fontSize = ScreenSpec.standbyTimeSp(formFactor).sp,
                                fontWeight = FontWeight.Light,
                                textAlign = TextAlign.Center,
                                modifier = Modifier.align(Alignment.Center),
                            )
                            DoorStatusChip(
                                doorState = state.doorState,
                                modifier = Modifier.align(Alignment.CenterEnd),
                            )
                        }
                        StandbyHostEnvRow(
                            readings = envReadings,
                            formFactor = formFactor,
                            modifier = Modifier.padding(top = 10.dp),
                        )
                    }
                }

                val bottomSectionHeight = screenHeight * ScreenSpec.standbyBottomSectionFraction(formFactor)
                val probeCardHeight = minOf(
                    screenHeight * ScreenSpec.HOME_CARD_HEIGHT_FRACTION,
                    bottomSectionHeight,
                )
                val messageListTopInset =
                    (bottomSectionHeight - probeCardHeight) / 2 + ScreenSpec.standbyProbeNameRowTopInset
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(bottomSectionHeight)
                        .padding(horizontal = ScreenSpec.homeHorizontalPadding),
                ) {
                    Box(
                        modifier = Modifier
                            .weight(if (showStandbyMessages) ScreenSpec.STANDBY_PROBE_COLUMN_WEIGHT else 1f)
                            .fillMaxHeight(),
                        contentAlignment = Alignment.Center,
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(probeCardHeight),
                        ) {
                            if (state.slaveProbes.isEmpty()) {
                                SlaveProbeCard(
                                    probe = SlaveProbeUi(id = "0", name = noProbeText, isOnline = false),
                                    onDetailClick = {},
                                    standbyFrosted = true,
                                    standbyWithMessages = showStandbyMessages,
                                    modifier = Modifier.fillMaxSize(),
                                )
                            } else {
                                HorizontalPager(
                                    state = pagerState,
                                    modifier = Modifier.fillMaxSize(),
                                ) { page ->
                                    SlaveProbeCard(
                                        probe = displayProbes[page],
                                        onDetailClick = {},
                                        standbyFrosted = true,
                                        standbyWithMessages = showStandbyMessages,
                                        modifier = Modifier.fillMaxSize(),
                                    )
                                }
                            }
                            if (displayProbes.size > 1) {
                                ProbePageIndicator(
                                    pageCount = displayProbes.size,
                                    currentPage = pagerState.currentPage,
                                    modifier = Modifier
                                        .align(Alignment.BottomCenter)
                                        .padding(bottom = 12.dp),
                                )
                            }
                        }
                    }

                    if (showStandbyMessages) {
                        Box(
                            modifier = Modifier
                                .weight(ScreenSpec.STANDBY_MESSAGE_COLUMN_WEIGHT)
                                .fillMaxHeight(),
                            contentAlignment = Alignment.TopStart,
                        ) {
                            StandbyMessageList(
                                messages = latestMessages,
                                noMessagesText = noMessagesText,
                                modifier = Modifier
                                    .fillMaxWidth(0.92f)
                                    .fillMaxHeight()
                                    .padding(top = messageListTopInset)
                                    .verticalScroll(rememberScrollState()),
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun StandbyHostEnvRow(
    readings: List<Pair<String, String>>,
    formFactor: TabletFormFactor,
    modifier: Modifier = Modifier,
) {
    if (readings.isEmpty()) return
    val envSp = ScreenSpec.STANDBY_HOST_ENV_ROW_SP
    val gap = ScreenSpec.standbyHostEnvRowGap(formFactor)
    Box(
        modifier = modifier.fillMaxWidth(),
        contentAlignment = Alignment.Center,
    ) {
        Row(
            modifier = Modifier.horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(gap),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            readings.forEach { (label, value) ->
                Text(
                    text = "$label $value",
                    color = RaylinkTextPrimary,
                    fontSize = envSp.sp,
                    maxLines = 1,
                )
            }
        }
    }
}

/** 下区右侧：标题「留言」+ 最新三条正文，每条淡背景、自动换行。 */
@Composable
private fun StandbyMessageList(
    messages: List<AlbumMessage>,
    noMessagesText: String,
    modifier: Modifier = Modifier,
) {
    val formFactor = rememberTabletFormFactor()
    val titleSp = ScreenSpec.standbyMessageTitleSp(formFactor)

    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.Start,
    ) {
        Text(
            text = stringResource(R.string.standby_messages_title),
            color = RaylinkTextSecondary,
            fontSize = titleSp.sp,
            fontWeight = FontWeight.Normal,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Start,
        )
        Spacer(modifier = Modifier.height(20.dp))
        if (messages.isEmpty()) {
            StandbyMessageItem(body = noMessagesText)
        } else {
            messages.forEachIndexed { index, message ->
                if (index > 0) {
                    Spacer(modifier = Modifier.height(20.dp))
                }
                StandbyMessageItem(body = message.text)
            }
        }
    }
}

@Composable
private fun StandbyMessageItem(
    body: String,
    modifier: Modifier = Modifier,
) {
    val formFactor = rememberTabletFormFactor()
    val messageSp = ScreenSpec.standbyMessageSp(formFactor)
    Text(
        text = body,
        color = RaylinkTextPrimary,
        fontSize = messageSp.sp,
        lineHeight = (messageSp + 8).sp,
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(RaylinkStandbyMessageBg)
            .padding(horizontal = 18.dp, vertical = 14.dp),
    )
}

@Preview(widthDp = 1280, heightDp = 800, showBackground = true)
@Composable
private fun StandbyScreenPreview() {
    RaylinkTheme {
        StandbyScreen(
            state = HomePreviewData.sampleOnline(),
            albumSettings = AlbumSettings(),
            messages = listOf(
                AlbumMessage(id = 1L, text = "123457890", createdAtMillis = 3L),
                AlbumMessage(id = 2L, text = "12345678933333\n333", createdAtMillis = 2L),
                AlbumMessage(id = 3L, text = "123456", createdAtMillis = 1L),
            ),
            timeSettings = TimeSettings(),
            onExit = {},
        )
    }
}
