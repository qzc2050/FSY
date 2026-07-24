package com.raydose.raylink.ui.standby

import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.requiredHeight
import androidx.compose.foundation.layout.requiredWidth
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.BlurredEdgeTreatment
import androidx.compose.ui.draw.blur
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import com.raydose.raylink.model.AlbumSettings

private val StandbyFrostedBlurRadius = 28.dp
private val StandbyFrostedBlurRadiusStrong = 48.dp
private val StandbyFrostedTint = Color(0x99101828)
private val StandbyFrostedTintFallback = Color(0xCC101828)
/** 探头区：轻蒙层，突出背景模糊 */
private val StandbyFrostedTintProbe = Color(0x28FFFFFF)

enum class StandbyFrostedStyle {
    /** 默认半透明底 */
    Default,
    /** 探头卡片：几乎全透明，仅保留强模糊 */
    ProbeFullBlur,
}

/**
 * 待机页毛玻璃容器：对齐全屏背景后模糊，再叠半透明色；前景内容保持清晰。
 */
@Composable
fun StandbyFrostedPanel(
    albumSettings: AlbumSettings,
    screenWidth: Dp,
    screenHeight: Dp,
    panelOffsetX: Dp,
    panelOffsetY: Dp,
    modifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(20.dp),
    style: StandbyFrostedStyle = StandbyFrostedStyle.Default,
    content: @Composable BoxScope.() -> Unit,
) {
    val density = LocalDensity.current
    val offsetXPx = with(density) { panelOffsetX.roundToPx() }
    val offsetYPx = with(density) { panelOffsetY.roundToPx() }
    val supportsBlur = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
    val blurRadius = when (style) {
        StandbyFrostedStyle.ProbeFullBlur -> StandbyFrostedBlurRadiusStrong
        StandbyFrostedStyle.Default -> StandbyFrostedBlurRadius
    }
    val tintColor = when {
        !supportsBlur -> StandbyFrostedTintFallback
        style == StandbyFrostedStyle.ProbeFullBlur -> StandbyFrostedTintProbe
        else -> StandbyFrostedTint
    }

    Box(
        modifier = modifier.clip(shape),
    ) {
        Box(
            modifier = Modifier
                .requiredWidth(screenWidth)
                .requiredHeight(screenHeight)
                .offset { IntOffset(-offsetXPx, -offsetYPx) }
                .then(
                    if (supportsBlur) {
                        Modifier.blur(
                            radius = blurRadius,
                            edgeTreatment = BlurredEdgeTreatment.Unbounded,
                        )
                    } else {
                        Modifier
                    },
                ),
        ) {
            StandbyBackground(albumSettings = albumSettings, scrimAlpha = 0f)
        }
        Box(
            modifier = Modifier
                .matchParentSize()
                .background(tintColor),
        )
        content()
    }
}
