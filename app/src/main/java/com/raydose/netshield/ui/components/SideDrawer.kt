package com.raydose.netshield.ui.components

import androidx.annotation.DrawableRes
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.R
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.ScreenSpec
import kotlinx.coroutines.launch

/** 侧边栏图标浅色（贴近原型淡黄白） */
private val SideDrawerIconTint = Color(0xFFF0E6C8)

/** 与主页探头卡片对齐的侧栏区域（宽=卡片右缘至屏右缘，高=卡片高） */
data class SideDrawerLayout(
    val panelWidth: Dp,
    val panelHeight: Dp,
    val panelTop: Dp,
)

enum class SideDrawerDestination {
    Music,
    Album,
    Files,
    Settings,
}

/** 侧栏滑动手势状态（参考 testandroid OverlayPanels） */
class SideDrawerPanelState(initialOpen: Boolean = false) {
    var isOpen by mutableStateOf(initialOpen)
    var dragOffset by mutableFloatStateOf(0f)
    /** 手势滑入已到位，避免松手后再播一次入场动画 */
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
fun rememberSideDrawerPanelState(initialOpen: Boolean = false): SideDrawerPanelState =
    remember { SideDrawerPanelState(initialOpen) }

@Composable
fun SideDrawerWithGesture(
    panelState: SideDrawerPanelState,
    layout: SideDrawerLayout,
    onOpenChanged: (Boolean) -> Unit,
    onDestinationClick: (SideDrawerDestination) -> Unit,
    modifier: Modifier = Modifier,
) {
    val density = LocalDensity.current
    val panelWidthPx = with(density) { layout.panelWidth.toPx() }
    val openThresholdPx = with(density) { 56.dp.toPx() }
    val panelTopPx = with(density) { layout.panelTop.roundToPx() }

    val showPanel = panelState.isOpen || panelState.dragOffset < 0f

    Box(modifier = modifier.fillMaxSize()) {
        if (panelState.isOpen) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.35f))
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
                    .align(Alignment.TopEnd)
                    .offset(y = layout.panelTop)
                    .width(layout.panelWidth)
                    .height(layout.panelHeight)
                    .pointerInput(Unit) {
                        detectHorizontalDragGestures(
                            onDragEnd = {
                                if (panelState.dragOffset < -openThresholdPx) {
                                    panelState.markOpenedFromDrag()
                                    panelState.isOpen = true
                                    onOpenChanged(true)
                                }
                                panelState.clearDragOffset()
                            },
                            onHorizontalDrag = { _, dragAmount ->
                                panelState.dragOffset = (panelState.dragOffset + dragAmount)
                                    .coerceIn(-panelWidthPx, 0f)
                            },
                        )
                    },
            )
        }

        if (showPanel) {
            val previewOffsetX = if (panelState.isOpen) 0f else (panelWidthPx + panelState.dragOffset)
            SideDrawerPanelContent(
                panelState = panelState,
                layout = layout,
                panelTopPx = panelTopPx,
                baseOffsetX = previewOffsetX,
                draggable = panelState.isOpen,
                onDismiss = {
                    panelState.isOpen = false
                    panelState.reset()
                    onOpenChanged(false)
                },
                onDestinationClick = onDestinationClick,
            )
        }
    }
}

@Composable
private fun SideDrawerPanelContent(
    panelState: SideDrawerPanelState,
    layout: SideDrawerLayout,
    panelTopPx: Int,
    baseOffsetX: Float,
    draggable: Boolean,
    onDismiss: () -> Unit,
    onDestinationClick: (SideDrawerDestination) -> Unit,
) {
    val density = LocalDensity.current
    val panelWidthPx = with(density) { layout.panelWidth.toPx() }
    val scope = rememberCoroutineScope()
    var dragDismissX by remember { mutableFloatStateOf(0f) }
    val slideOffset = remember(panelWidthPx) { Animatable(panelWidthPx) }

    // 跟手预览：直接同步位移，不播放入场动画
    LaunchedEffect(baseOffsetX, draggable) {
        if (!draggable) {
            slideOffset.snapTo(baseOffsetX.coerceIn(0f, panelWidthPx))
        }
    }

    // 正式打开：仅点击打开时从屏外滑入；手势滑入到位后不再二次弹出
    LaunchedEffect(draggable, panelWidthPx) {
        if (!draggable) return@LaunchedEffect
        dragDismissX = 0f
        if (panelState.skipNextOpenAnimation) {
            panelState.skipNextOpenAnimation = false
            slideOffset.snapTo(0f)
        } else {
            slideOffset.snapTo(panelWidthPx)
            slideOffset.animateTo(0f, tween(280))
        }
    }

    val offsetX = slideOffset.value + if (draggable) dragDismissX else 0f

    Box(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .offset { IntOffset(x = offsetX.toInt(), y = panelTopPx) }
                .width(layout.panelWidth)
                .height(layout.panelHeight)
                .clip(RoundedCornerShape(20.dp))
                .background(Color(0xE61A1B3A))
                .padding(vertical = 16.dp, horizontal = 6.dp)
                .pointerInput(draggable) {
                    if (!draggable) return@pointerInput
                    detectHorizontalDragGestures(
                        onDragEnd = {
                            scope.launch {
                                if (dragDismissX > 80f) {
                                    dragDismissX = 0f
                                    onDismiss()
                                } else {
                                    Animatable(dragDismissX).animateTo(0f, tween(200))
                                    dragDismissX = 0f
                                }
                            }
                        },
                        onHorizontalDrag = { _, dragAmount ->
                            dragDismissX = (dragDismissX + dragAmount).coerceAtLeast(0f)
                        },
                    )
                },
            verticalArrangement = Arrangement.SpaceEvenly,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            DrawerItem(R.drawable.ic_sidebar_music, "音乐播放") {
                onDestinationClick(SideDrawerDestination.Music)
            }
            DrawerItem(R.drawable.ic_sidebar_album, "电子相册") {
                onDestinationClick(SideDrawerDestination.Album)
            }
            DrawerItem(R.drawable.ic_sidebar_folder, "本地文件") {
                onDestinationClick(SideDrawerDestination.Files)
            }
            DrawerItem(R.drawable.ic_sidebar_settings, "系统设置") {
                onDestinationClick(SideDrawerDestination.Settings)
            }
        }
    }
}

@Composable
private fun DrawerItem(
    @DrawableRes iconRes: Int,
    label: String,
    onClick: () -> Unit,
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier
            .clickable(onClick = onClick)
            .padding(vertical = 8.dp, horizontal = 4.dp),
    ) {
        Icon(
            painter = painterResource(iconRes),
            contentDescription = label,
            tint = SideDrawerIconTint,
            modifier = Modifier.size(ScreenSpec.sideDrawerIconSize),
        )
        Text(
            text = label,
            color = NetShieldTextPrimary,
            fontSize = ScreenSpec.SIDE_DRAWER_LABEL_SP.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(top = 8.dp),
        )
    }
}
