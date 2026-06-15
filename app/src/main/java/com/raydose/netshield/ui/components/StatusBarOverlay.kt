package com.raydose.netshield.ui.components

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import com.raydose.netshield.R
import com.raydose.netshield.model.ConnectedDeviceUi
import com.raydose.netshield.model.SystemAlertLog
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import kotlinx.coroutines.launch
import kotlin.math.abs

/** 顶部下拉面板手势状态（参考 testandroid TopPanelWithGesture） */
class StatusBarPanelState(initialOpen: Boolean = false) {
    var isOpen by mutableStateOf(initialOpen)
    var dragOffset by mutableFloatStateOf(0f)
    var skipNextOpenAnimation by mutableStateOf(false)

    fun markOpenedFromDrag() {
        skipNextOpenAnimation = true
    }

    fun clearDragOffset() {
        dragOffset = 0f
    }

    fun reset() {
        dragOffset = 0f
        skipNextOpenAnimation = false
    }
}

@Composable
fun rememberStatusBarPanelState(initialOpen: Boolean = false): StatusBarPanelState =
    remember { StatusBarPanelState(initialOpen) }

/** 未展开时的跟手预览：上下均可拖，松手按阈值决定展开或收回 */
private fun Modifier.statusBarPreviewDrag(
    panelState: StatusBarPanelState,
    panelHeightPx: Float,
    openThresholdPx: Float,
    onOpenChanged: (Boolean) -> Unit,
): Modifier = pointerInput(panelState.isOpen) {
    if (panelState.isOpen) return@pointerInput
    detectVerticalDragGestures(
        onDragEnd = {
            if (panelState.dragOffset > openThresholdPx) {
                panelState.markOpenedFromDrag()
                panelState.isOpen = true
                onOpenChanged(true)
            }
            panelState.clearDragOffset()
        },
        onVerticalDrag = { _, dragAmount ->
            panelState.dragOffset = (panelState.dragOffset + dragAmount)
                .coerceIn(0f, panelHeightPx)
        },
    )
}

/**
 * 顶栏下拉带：全区域可下拉；箭头热区内轻触为点击展开。
 * 箭头层不再单独 clickable，避免拦截下拉手势。
 */
private fun Modifier.homePullDownGestureZone(
    panelState: StatusBarPanelState,
    panelHeightPx: Float,
    openThresholdPx: Float,
    hintTapWidthPx: Float,
    hintTapHeightPx: Float,
    onOpenChanged: (Boolean) -> Unit,
    onHintTap: () -> Unit,
): Modifier = pointerInput(panelState.isOpen, hintTapWidthPx, hintTapHeightPx) {
    if (panelState.isOpen) return@pointerInput
    val touchSlop = viewConfiguration.touchSlop

    fun hintTapRect(zoneSize: IntSize): Rect {
        val left = (zoneSize.width - hintTapWidthPx) / 2f
        val top = (zoneSize.height - hintTapHeightPx) / 2f
        return Rect(left, top, left + hintTapWidthPx, top + hintTapHeightPx)
    }

    awaitEachGesture {
        val down = awaitFirstDown(requireUnconsumed = false)
        var accumulatedY = 0f
        var dragging = false

        while (true) {
            val event = awaitPointerEvent()
            val change = event.changes.firstOrNull { it.id == down.id } ?: break

            if (!change.pressed) {
                if (dragging) {
                    if (panelState.dragOffset > openThresholdPx) {
                        panelState.markOpenedFromDrag()
                        panelState.isOpen = true
                        onOpenChanged(true)
                    }
                    panelState.clearDragOffset()
                } else if (hintTapRect(size).contains(down.position)) {
                    onHintTap()
                }
                break
            }

            val deltaY = change.positionChange().y
            if (!dragging) {
                accumulatedY += deltaY
                if (abs(accumulatedY) > touchSlop) {
                    dragging = true
                }
            }
            if (dragging && deltaY != 0f) {
                panelState.dragOffset = (panelState.dragOffset + deltaY)
                    .coerceIn(0f, panelHeightPx)
                change.consume()
            }
        }
    }
}

@Composable
fun StatusBarWithGesture(
    panelState: StatusBarPanelState,
    panelHeight: Dp,
    wifiName: String,
    wifiPassword: String,
    bluetoothName: String,
    connectedDevices: List<ConnectedDeviceUi>,
    alertLogs: List<SystemAlertLog>,
    onOpenChanged: (Boolean) -> Unit,
    thumbnailContent: @Composable () -> Unit,
    modifier: Modifier = Modifier,
) {
    val density = LocalDensity.current
    val panelHeightPx = with(density) { panelHeight.toPx() }
    val openThresholdPx = with(density) { 56.dp.toPx() }
    val hintTapWidthPx = with(density) { ScreenSpec.homePullDownHintTapWidth.toPx() }
    val hintTapHeightPx = with(density) { ScreenSpec.homePullDownHintTapHeight.toPx() }
    val scope = rememberCoroutineScope()
    val hintPulseScale = remember { Animatable(1f) }
    val pullProgress = (panelState.dragOffset / openThresholdPx).coerceIn(0f, 1f)
    val showPanel = panelState.isOpen || panelState.dragOffset > 0f

    val onHintTap: () -> Unit = {
        scope.launch {
            hintPulseScale.snapTo(1f)
            hintPulseScale.animateTo(1.38f, tween(100))
            hintPulseScale.animateTo(1f, tween(180))
        }
        panelState.isOpen = true
        onOpenChanged(true)
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val screenHeight = maxHeight
        val currentPanelHeight = when {
            panelState.isOpen -> panelHeight
            panelState.dragOffset > 0f -> with(density) { panelState.dragOffset.toDp() }
            else -> 0.dp
        }
        val thumbnailAreaHeight = (screenHeight - currentPanelHeight).coerceAtLeast(0.dp)

        if (panelState.isOpen) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.5f))
                    .clickable {
                        panelState.isOpen = false
                        panelState.reset()
                        onOpenChanged(false)
                    },
            )
        }

        if (!panelState.isOpen) {
            Box(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .fillMaxWidth()
                    .height(ScreenSpec.statusBarGestureZoneHeight)
                    .homePullDownGestureZone(
                        panelState = panelState,
                        panelHeightPx = panelHeightPx,
                        openThresholdPx = openThresholdPx,
                        hintTapWidthPx = hintTapWidthPx,
                        hintTapHeightPx = hintTapHeightPx,
                        onOpenChanged = onOpenChanged,
                        onHintTap = onHintTap,
                    ),
                contentAlignment = Alignment.Center,
            ) {
                PullDownHint(
                    pullProgress = pullProgress,
                    clickPulseScale = hintPulseScale.value,
                )
            }
        }

        if (showPanel && thumbnailAreaHeight > 0.dp) {
            Box(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .height(thumbnailAreaHeight)
                    .clipToBounds()
                    .then(
                        if (panelState.isOpen) {
                            Modifier.clickable {
                                panelState.isOpen = false
                                panelState.reset()
                                onOpenChanged(false)
                            }
                        } else {
                            Modifier
                        },
                    ),
            ) {
                Box(modifier = Modifier.fillMaxSize()) {
                    thumbnailContent()
                }
            }
        }

        if (showPanel) {
            val previewOffsetY =
                if (panelState.isOpen) 0f else (-panelHeightPx + panelState.dragOffset)
            StatusBarPanelContent(
                panelState = panelState,
                panelHeight = panelHeight,
                panelHeightPx = panelHeightPx,
                openThresholdPx = openThresholdPx,
                baseOffsetY = previewOffsetY,
                draggable = panelState.isOpen,
                onOpenChanged = onOpenChanged,
                wifiName = wifiName,
                wifiPassword = wifiPassword,
                bluetoothName = bluetoothName,
                connectedDevices = connectedDevices,
                alertLogs = alertLogs,
                onDismiss = {
                    panelState.isOpen = false
                    panelState.reset()
                    onOpenChanged(false)
                },
            )
        }
    }
}

@Composable
private fun StatusBarPanelContent(
    panelState: StatusBarPanelState,
    panelHeight: Dp,
    panelHeightPx: Float,
    openThresholdPx: Float,
    baseOffsetY: Float,
    draggable: Boolean,
    onOpenChanged: (Boolean) -> Unit,
    wifiName: String,
    wifiPassword: String,
    bluetoothName: String,
    connectedDevices: List<ConnectedDeviceUi>,
    alertLogs: List<SystemAlertLog>,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var dragDismissY by remember { mutableFloatStateOf(0f) }
    val slideOffset = remember(panelHeightPx) { Animatable(-panelHeightPx) }
    val dismissThresholdPx = openThresholdPx

    LaunchedEffect(baseOffsetY, draggable) {
        if (!draggable) {
            slideOffset.snapTo(baseOffsetY.coerceIn(-panelHeightPx, 0f))
        }
    }

    LaunchedEffect(draggable, panelHeightPx) {
        if (!draggable) return@LaunchedEffect
        dragDismissY = 0f
        if (panelState.skipNextOpenAnimation) {
            panelState.skipNextOpenAnimation = false
            slideOffset.snapTo(0f)
        } else {
            slideOffset.snapTo(-panelHeightPx)
            slideOffset.animateTo(0f, tween(280))
        }
    }

    val offsetY = slideOffset.value + if (draggable) dragDismissY else 0f

    val panelModifier = Modifier
        .fillMaxWidth()
        .height(panelHeight)
        .offset { IntOffset(x = 0, y = offsetY.toInt()) }
        .background(Color(0xEE1A1B3A))
        .then(
            if (!draggable) {
                Modifier.statusBarPreviewDrag(panelState, panelHeightPx, openThresholdPx, onOpenChanged)
            } else {
                Modifier.pointerInput(Unit) {
                    detectVerticalDragGestures(
                        onDragEnd = {
                            scope.launch {
                                if (dragDismissY < -dismissThresholdPx) {
                                    dragDismissY = 0f
                                    onDismiss()
                                } else {
                                    Animatable(dragDismissY).animateTo(0f, tween(200))
                                    dragDismissY = 0f
                                }
                            }
                        },
                        onVerticalDrag = { _, dragAmount ->
                            dragDismissY = (dragDismissY + dragAmount)
                                .coerceIn(-panelHeightPx, 0f)
                        },
                    )
                }
            },
        )
        .padding(top = 8.dp, bottom = 12.dp)

    Box(modifier = panelModifier) {
        StatusBarPanelBody(
            wifiName = wifiName,
            wifiPassword = wifiPassword,
            bluetoothName = bluetoothName,
            connectedDevices = connectedDevices,
            alertLogs = alertLogs,
            modifier = Modifier.fillMaxSize(),
        )
        Text(
            text = "✕",
            color = NetShieldTextPrimary,
            fontSize = 22.sp,
            modifier = Modifier
                .align(Alignment.TopEnd)
                .clickable(onClick = onDismiss)
                .padding(8.dp),
        )
    }
}

@Composable
fun PullDownHint(
    pullProgress: Float = 0f,
    clickPulseScale: Float = 1f,
    modifier: Modifier = Modifier,
) {
    val stretchY = 1f + pullProgress * 1.5f
    val stretchX = 1f + pullProgress * 0.18f
    val scaleX = stretchX * clickPulseScale
    val scaleY = stretchY * clickPulseScale
    val transformOrigin = if (pullProgress > 0.01f) {
        TransformOrigin(0.5f, 0f)
    } else {
        TransformOrigin(0.5f, 0.5f)
    }

    Box(
        modifier = modifier.size(
            width = ScreenSpec.homePullDownHintTapWidth,
            height = ScreenSpec.homePullDownHintTapHeight,
        ),
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_home_hint_chevron_down),
            contentDescription = "展开状态栏",
            tint = NetShieldTextSecondary,
            modifier = Modifier
                .size(
                    width = ScreenSpec.homePullDownHintVisualWidth,
                    height = ScreenSpec.homePullDownHintVisualHeight,
                )
                .graphicsLayer {
                    this.scaleX = scaleX
                    this.scaleY = scaleY
                    this.transformOrigin = transformOrigin
                },
        )
    }
}

@Composable
fun SideSwipeHint(
    swipeProgress: Float = 0f,
    clickPulseScale: Float = 1f,
    modifier: Modifier = Modifier,
) {
    val stretchX = 1f + swipeProgress * 1.5f
    val stretchY = 1f + swipeProgress * 0.18f
    val scaleX = stretchX * clickPulseScale
    val scaleY = stretchY * clickPulseScale
    val transformOrigin = if (swipeProgress > 0.01f) {
        TransformOrigin(1f, 0.5f)
    } else {
        TransformOrigin(0.5f, 0.5f)
    }

    Box(
        modifier = modifier.size(
            width = ScreenSpec.homeSideSwipeHintTapWidth,
            height = ScreenSpec.homeSideSwipeHintTapHeight,
        ),
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_home_hint_chevron_left),
            contentDescription = "打开侧栏",
            tint = NetShieldTextSecondary,
            modifier = Modifier
                .size(
                    width = ScreenSpec.homeSideSwipeHintVisualWidth,
                    height = ScreenSpec.homeSideSwipeHintVisualHeight,
                )
                .graphicsLayer {
                    this.scaleX = scaleX
                    this.scaleY = scaleY
                    this.transformOrigin = transformOrigin
                },
        )
    }
}

/**
 * 探头卡片侧滑带：全区域可左滑；箭头热区内轻触为点击展开侧栏。
 */
fun Modifier.homeSideSwipeGestureZone(
    panelState: SideDrawerPanelState,
    drawerPanelWidthPx: Float,
    openThresholdPx: Float,
    hintTapWidthPx: Float,
    hintTapHeightPx: Float,
    hintEndPaddingPx: Float,
    onOpen: () -> Unit,
    onHintTap: () -> Unit,
): Modifier = pointerInput(
    panelState.isOpen,
    hintTapWidthPx,
    hintTapHeightPx,
    hintEndPaddingPx,
) {
    if (panelState.isOpen) return@pointerInput
    val touchSlop = viewConfiguration.touchSlop

    fun hintTapRect(zoneSize: IntSize): Rect {
        val left = zoneSize.width - hintEndPaddingPx - hintTapWidthPx
        val top = (zoneSize.height - hintTapHeightPx) / 2f
        return Rect(left, top, left + hintTapWidthPx, top + hintTapHeightPx)
    }

    awaitEachGesture {
        val down = awaitFirstDown(requireUnconsumed = false)
        var accumulatedX = 0f
        var dragging = false

        while (true) {
            val event = awaitPointerEvent()
            val change = event.changes.firstOrNull { it.id == down.id } ?: break

            if (!change.pressed) {
                if (dragging) {
                    if (panelState.dragOffset < -openThresholdPx) {
                        panelState.markOpenedFromDrag()
                        onOpen()
                    }
                    panelState.clearDragOffset()
                } else if (hintTapRect(size).contains(down.position)) {
                    onHintTap()
                }
                break
            }

            val deltaX = change.positionChange().x
            if (!dragging) {
                accumulatedX += deltaX
                if (abs(accumulatedX) > touchSlop) {
                    dragging = true
                }
            }
            if (dragging && deltaX != 0f) {
                panelState.dragOffset = (panelState.dragOffset + deltaX)
                    .coerceIn(-drawerPanelWidthPx, 0f)
                change.consume()
            }
        }
    }
}
