package com.raydose.netshield.ui.components

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.material3.Text
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import kotlinx.coroutines.launch

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
    val showPanel = panelState.isOpen || panelState.dragOffset > 0f

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val screenHeight = maxHeight
        val thumbnailMaxHeight = screenHeight * ScreenSpec.statusBarThumbnailHeightFraction()
        val currentPanelHeight = when {
            panelState.isOpen -> panelHeight
            panelState.dragOffset > 0f -> with(density) { panelState.dragOffset.toDp() }
            else -> 0.dp
        }
        val thumbnailAreaHeight = (screenHeight - currentPanelHeight).coerceAtLeast(0.dp)
        val thumbnailScale = if (screenHeight > 0.dp) {
            (thumbnailAreaHeight.value / screenHeight.value).coerceIn(0.01f, 1f)
        } else {
            1f
        }

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
                    .statusBarPreviewDrag(panelState, panelHeightPx, openThresholdPx, onOpenChanged),
            )
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
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(screenHeight)
                        .graphicsLayer {
                            scaleX = thumbnailScale
                            scaleY = thumbnailScale
                            transformOrigin = TransformOrigin(0.5f, 0f)
                        },
                ) {
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
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Text(
        text = "⌄",
        color = NetShieldTextSecondary,
        fontSize = 20.sp,
        modifier = modifier
            .clickable(onClick = onClick)
            .padding(4.dp),
    )
}

@Composable
fun SideSwipeHint(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Text(
        text = "›",
        color = NetShieldTextSecondary,
        fontSize = 28.sp,
        fontWeight = FontWeight.Light,
        modifier = modifier
            .clickable(onClick = onClick)
            .padding(8.dp),
    )
}
