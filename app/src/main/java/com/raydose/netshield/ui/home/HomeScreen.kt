package com.raydose.netshield.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
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
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.AlertLogKind
import com.raydose.netshield.model.DoorState
import com.raydose.netshield.model.HomeUiState
import com.raydose.netshield.model.MessageItem
import com.raydose.netshield.model.ProbeCardDisplayMode
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.model.SystemAlertLog
import com.raydose.netshield.model.statusBarConnectedDevices
import com.raydose.netshield.ui.components.DateTimeColumn
import com.raydose.netshield.ui.components.DoorStatusChip
import com.raydose.netshield.ui.components.GradientBackground
import com.raydose.netshield.ui.components.HomeTopBar
import com.raydose.netshield.ui.components.HostEnvScrollPanel
import com.raydose.netshield.ui.components.MessageTickerBar
import com.raydose.netshield.ui.components.MessageEditDialog
import com.raydose.netshield.ui.components.ProbePagerAutoScroll
import com.raydose.netshield.ui.components.ProbePageIndicator
import com.raydose.netshield.ui.components.SideDrawerDestination
import com.raydose.netshield.ui.components.SideDrawerLayout
import com.raydose.netshield.ui.components.SideDrawerWithGesture
import com.raydose.netshield.ui.components.rememberSideDrawerPanelState
import com.raydose.netshield.ui.components.SlaveProbeCard
import com.raydose.netshield.ui.components.StatusBarWithGesture
import com.raydose.netshield.ui.components.rememberStatusBarPanelState
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import kotlinx.coroutines.delay
object HomePreviewData {
    fun sampleOffline(): HomeUiState = HomeUiState(
        dateText = "2026年05月29日  农历四月十三",
        timeText = "10:30:46",
        hostEnvReadings = listOf(
            com.raydose.netshield.model.HostEnvReading("温度", "26°C"),
            com.raydose.netshield.model.HostEnvReading("湿度", "65%"),
            com.raydose.netshield.model.HostEnvReading("CO2", "420 ppm"),
            com.raydose.netshield.model.HostEnvReading("气压", "101.2 kPa"),
        ),
        slaveProbes = listOf(
            SlaveProbeUi(id = "1", name = "Detector 1", isOnline = false),
        ),
        doorState = DoorState.Open,
        messages = listOf(MessageItem(1, "示例留言内容")),
    )

    fun sampleOnline(): HomeUiState = HomeUiState(
        dateText = "2026年05月08日  农历四月初一",
        timeText = "PM 02:25:25",
        hostEnvReadings = listOf(
            com.raydose.netshield.model.HostEnvReading("温度", "28°C"),
            com.raydose.netshield.model.HostEnvReading("湿度", "32%"),
            com.raydose.netshield.model.HostEnvReading("CO2", "154 ppm"),
            com.raydose.netshield.model.HostEnvReading("气压", "101.33 kPa"),
        ),
        alertLogs = sampleAlertLogs(),
        slaveProbes = listOf(
            SlaveProbeUi(
                id = "1",
                name = "FSY-I",
                ip = "192.168.1.101",
                isOnline = true,
                doseRateText = "0.37",
                temperature = "30.4°C",
                pressure = "101.76 kPa",
                humidity = "32%",
                co2 = "154 ppm",
                pm25 = "37.0 μg/m³",
                hasAlarm = true,
            ),
            SlaveProbeUi(id = "2", name = "FSY-II", ip = "192.168.1.102", isOnline = true, doseRateText = "0.42"),
            SlaveProbeUi(id = "3", name = "FSY-III", ip = "192.168.1.103", isOnline = false),
            SlaveProbeUi(id = "4", name = "FSY-IV", ip = "192.168.1.104", isOnline = true, doseRateText = "0.21"),
        ),
        doorState = DoorState.Open,
        latestAlert = "Detector 3 数值报警",
        messages = listOf(MessageItem(1, "留言示例")),
    )
}

@Composable
fun HomeScreen(
    state: HomeUiState,
    probeCardMode: ProbeCardDisplayMode = ProbeCardDisplayMode.Fixed,
    visibleProbeCards: Int = 1,
    onStatusBarToggle: () -> Unit,
    onSideDrawerToggle: () -> Unit,
    onSideDrawerDismiss: () -> Unit,
    onSideDrawerDestination: (SideDrawerDestination) -> Unit,
    onStatusBarDismiss: () -> Unit,
    onProbeDetailClick: (String) -> Unit,
    onMessageBarClick: () -> Unit,
    onAddMessage: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val probePagerKey = probeDisplayListKey(state.slaveProbes)
    val pagerConfig = remember(probePagerKey, visibleProbeCards, probeCardMode) {
        resolveHomeProbePagerConfig(state.slaveProbes, visibleProbeCards, probeCardMode)
    }
    val displayProbes = if (pagerConfig.alarmPriorityActive) {
        state.slaveProbes.filter { it.hasAlarm }
    } else {
        state.slaveProbes
    }
    val probesPerPage = pagerConfig.probesPerPage
    val probePageCount = pagerConfig.pageCount
    val autoScroll = pagerConfig.autoScroll
    val displayListKey = probePagerKey
    val pagerState = rememberPagerState(pageCount = { probePageCount.coerceAtLeast(1) })
    val drawerPanelState = rememberSideDrawerPanelState()
    val statusBarPanelState = rememberStatusBarPanelState()
    val density = LocalDensity.current
    var messageTickerResetKey by remember { mutableStateOf(0) }
    var tickerDisplayLine by remember { mutableStateOf("") }
    var showMessageList by remember { mutableStateOf(false) }
    var showAddMessageDialog by remember { mutableStateOf(false) }

    LaunchedEffect(state.sideDrawerOpen) {
        drawerPanelState.isOpen = state.sideDrawerOpen
        if (!state.sideDrawerOpen) drawerPanelState.reset()
    }

    LaunchedEffect(state.statusBarExpanded) {
        statusBarPanelState.isOpen = state.statusBarExpanded
        if (!state.statusBarExpanded) statusBarPanelState.reset()
    }

    LaunchedEffect(displayListKey) {
        if (pagerState.currentPage != 0) {
            pagerState.scrollToPage(0)
        }
    }

    LaunchedEffect(state.selectedProbeIndex, probesPerPage, pagerConfig.alarmPriorityActive) {
        if (displayProbes.isEmpty()) return@LaunchedEffect
        val targetPage = if (pagerConfig.alarmPriorityActive) {
            val probe = state.slaveProbes.getOrNull(state.selectedProbeIndex) ?: return@LaunchedEffect
            displayProbes.indexOfFirst { it.id == probe.id }.takeIf { it >= 0 } ?: 0
        } else {
            (state.selectedProbeIndex / probesPerPage).coerceIn(0, probePageCount - 1)
        }
        if (pagerState.currentPage != targetPage) {
            pagerState.animateScrollToPage(targetPage)
        }
    }

    ProbePagerAutoScroll(
        enabled = autoScroll,
        autoScrollPageIndices = pagerConfig.autoScrollPageIndices,
        pageCount = probePageCount,
        pagerState = pagerState,
        paused = state.statusBarExpanded,
    )

    LaunchedEffect(state.messages) {
        tickerDisplayLine = state.messages.firstOrNull()?.text?.lineSequence()?.firstOrNull().orEmpty()
            .ifBlank { if (state.messages.isEmpty()) "暂无留言" else "" }
    }

    LaunchedEffect(state.statusBarExpanded, state.messages) {
        if (state.statusBarExpanded || state.messages.isEmpty()) showMessageList = false
    }

    LaunchedEffect(state.messages.size) {
        messageTickerResetKey++
    }

    Box(modifier = modifier.fillMaxSize()) {
        GradientBackground()

        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val screenWidth = maxWidth
            val screenHeight = maxHeight
            val statusBarPanelHeight = screenHeight * ScreenSpec.STATUS_BAR_PANEL_HEIGHT_FRACTION
            val cardHeight = screenHeight * ScreenSpec.HOME_CARD_HEIGHT_FRACTION
            val footerBottomInset = screenHeight * ScreenSpec.HOME_FOOTER_BOTTOM_FRACTION
            val drawerGapWidth = screenWidth * ScreenSpec.homeSideDrawerWidthFraction()
            val drawerWidth = drawerGapWidth * ScreenSpec.SIDE_DRAWER_WIDTH_RATIO_OF_GAP
            val drawerHeight = cardHeight * ScreenSpec.sideDrawerHeightRatioOfCard(screenHeight)
            val drawerTopInset = cardHeight * ScreenSpec.sideDrawerTopInsetRatioOfCard(screenHeight)
            val drawerPanelWidthPx = with(density) { drawerWidth.toPx() }
            val drawerOpenThresholdPx = with(density) { 56.dp.toPx() }
            val messageBarWidthFraction = 0.60f
            val messagePopupWidthFraction = messageBarWidthFraction
            var cardTopInRoot by remember { mutableStateOf(0.dp) }
            val drawerLayout = SideDrawerLayout(
                panelWidth = drawerWidth,
                panelHeight = drawerHeight,
                panelTop = cardTopInRoot + drawerTopInset,
            )
            fun x(percent: Float) = screenWidth * percent

            Column(modifier = Modifier.fillMaxSize()) {
                HomeTopBar(
                    systemName = state.systemName,
                    bluetoothOnline = state.bluetoothOnline,
                    ethernetOnline = state.ethernetOnline,
                )

                if (!state.statusBarExpanded) {
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
                            autoScroll = autoScroll,
                        )
                    }
                }

                // 第三行起：卡片 + 圆点（居中于卡片与底栏之间）+ 底栏
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                ) {
                    if (state.statusBarExpanded) {
                        Spacer(modifier = Modifier.weight(1f))
                    } else {
                    Spacer(modifier = Modifier.height(ScreenSpec.homeCardTopGap))

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(cardHeight)
                            .onGloballyPositioned { coordinates ->
                                cardTopInRoot = with(density) {
                                    coordinates.positionInRoot().y.toDp()
                                }
                            }
                            .pointerInput(state.sideDrawerOpen) {
                                if (state.sideDrawerOpen) return@pointerInput
                                detectHorizontalDragGestures(
                                    onDragEnd = {
                                        if (drawerPanelState.dragOffset < -drawerOpenThresholdPx) {
                                            drawerPanelState.markOpenedFromDrag()
                                            onSideDrawerToggle()
                                        }
                                        drawerPanelState.clearDragOffset()
                                    },
                                    onHorizontalDrag = { _, dragAmount ->
                                        drawerPanelState.dragOffset =
                                            (drawerPanelState.dragOffset + dragAmount)
                                                .coerceIn(-drawerPanelWidthPx, 0f)
                                    },
                                )
                            },
                    ) {
                        Box(
                            modifier = Modifier
                                .align(Alignment.TopStart)
                                .offset(x = x(ScreenSpec.HOME_CARD_START_FRACTION))
                                .width(x(ScreenSpec.HOME_CARD_WIDTH_FRACTION))
                                .fillMaxHeight(),
                        ) {
                            if (state.slaveProbes.isEmpty()) {
                                SlaveProbeCard(
                                    probe = SlaveProbeUi(id = "0", name = "暂无探头", isOnline = false),
                                    onDetailClick = {},
                                    modifier = Modifier.fillMaxSize(),
                                )
                            } else {
                                HorizontalPager(
                                    state = pagerState,
                                    modifier = Modifier.fillMaxSize(),
                                ) { page ->
                                    HomeProbeGridPage(
                                        slots = homeProbeGridSlots(
                                            probes = displayProbes,
                                            page = page,
                                            visiblePerPage = probesPerPage,
                                        ),
                                        visiblePerPage = probesPerPage,
                                        onProbeDetailClick = onProbeDetailClick,
                                        modifier = Modifier.fillMaxSize(),
                                    )
                                }
                            }
                        }
                    }

                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f),
                        contentAlignment = Alignment.Center,
                    ) {
                        if (displayProbes.isNotEmpty() && probePageCount > 1) {
                            ProbePageIndicator(
                                pageCount = probePageCount,
                                currentPage = pagerState.currentPage,
                            )
                        }
                    }

                    if (!state.statusBarExpanded) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = ScreenSpec.homeHorizontalPadding),
                        ) {
                            DoorStatusChip(
                                doorState = state.doorState,
                                modifier = Modifier.align(Alignment.CenterStart),
                            )
                            MessageTickerBar(
                                messageTextAt = { idx ->
                                    state.messages.getOrNull(idx)?.text ?: "暂无留言"
                                },
                                messageCount = state.messages.size,
                                animateMessageChange = autoScroll && state.messages.size > 1,
                                resetKey = messageTickerResetKey,
                                onDisplayLineChange = { _, line ->
                                    tickerDisplayLine = line
                                },
                                onAddMessageClick = { showAddMessageDialog = true },
                                onClick = {
                                    if (state.messages.isNotEmpty()) {
                                        showMessageList = !showMessageList
                                    }
                                    onMessageBarClick()
                                },
                                modifier = Modifier
                                    .align(Alignment.Center)
                                    .fillMaxWidth(messageBarWidthFraction),
                                widthFraction = 1f,
                            )
                        }

                        Spacer(modifier = Modifier.height(footerBottomInset))
                    }
                    }
                }
            }

            SideDrawerWithGesture(
                panelState = drawerPanelState,
                layout = drawerLayout,
                onOpenChanged = { open ->
                    if (open && !state.sideDrawerOpen) onSideDrawerToggle()
                    if (!open && state.sideDrawerOpen) onSideDrawerDismiss()
                },
                onDestinationClick = onSideDrawerDestination,
                modifier = Modifier.fillMaxSize(),
            )

            StatusBarWithGesture(
                panelState = statusBarPanelState,
                panelHeight = statusBarPanelHeight,
                wifiName = state.hostNetwork.wifiName,
                wifiPassword = state.hostNetwork.wifiPassword,
                bluetoothName = state.hostNetwork.bluetoothName,
                connectedDevices = state.statusBarConnectedDevices(),
                alertLogs = state.alertLogs,
                onOpenChanged = { open ->
                    if (open && !state.statusBarExpanded) onStatusBarToggle()
                    if (!open && state.statusBarExpanded) onStatusBarDismiss()
                },
                thumbnailContent = {
                    HomeThumbnailContent(
                        state = state,
                        displayProbes = displayProbes,
                        probesPerPage = probesPerPage,
                        pagerState = pagerState,
                        currentMessageLine = tickerDisplayLine,
                    )
                },
                modifier = Modifier.fillMaxSize(),
            )

            // 作为根层浮层渲染，不参与底部栏测量，避免点击留言时底栏元素上跳。
            if (showMessageList && state.messages.isNotEmpty() && !state.statusBarExpanded) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.BottomCenter,
                ) {
                    HomeMessagePopup(
                        messages = state.messages,
                        modifier = Modifier
                            .fillMaxWidth(messagePopupWidthFraction)
                            .padding(bottom = footerBottomInset + 66.dp),
                    )
                }
            }

            if (showAddMessageDialog) {
                MessageEditDialog(
                    initialText = "",
                    isNew = true,
                    onDismiss = { showAddMessageDialog = false },
                    onConfirm = { text ->
                        val trimmed = text.trim()
                        if (trimmed.isNotEmpty()) {
                            onAddMessage(trimmed)
                            messageTickerResetKey++
                        }
                        showAddMessageDialog = false
                    },
                )
            }
        }
    }
}

@Composable
private fun HomeMessagePopup(
    messages: List<MessageItem>,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(18.dp))
            .background(Color(0xCC1B3555))
            .padding(horizontal = 10.dp, vertical = 7.dp),
    ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(max = 170.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            items(messages, key = { it.id }) { message ->
                Text(
                    text = message.text,
                    color = NetShieldTextPrimary,
                    fontSize = 17.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color.White.copy(alpha = 0.10f))
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                )
            }
        }
    }
}

private fun sampleAlertLogs(): List<SystemAlertLog> = listOf(
    SystemAlertLog(1, "2025-10-08 13:07:21", "Detector 3 数值报警", AlertLogKind.Alarm),
    SystemAlertLog(2, "2025-10-08 13:07:21", "电源断开", AlertLogKind.PowerOff),
    SystemAlertLog(3, "2025-10-08 13:07:21", "Detector 3 已连接", AlertLogKind.Connected),
    SystemAlertLog(4, "2025-10-08 13:07:21", "Detector 2 更名为 Detector 1", AlertLogKind.Rename),
    SystemAlertLog(5, "2025-10-08 13:07:18", "Detector 1 数值报警", AlertLogKind.Warning),
    SystemAlertLog(6, "2025-10-08 13:06:55", "电源恢复", AlertLogKind.Info),
    SystemAlertLog(7, "2025-10-08 13:05:40", "Detector 2 已连接", AlertLogKind.Connected),
    SystemAlertLog(8, "2025-10-08 13:04:12", "本机网络已连接", AlertLogKind.Info),
)

