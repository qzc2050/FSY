package com.raydose.netshield.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private val TrackHeight = 10.dp
private val ThumbSize = 26.dp
private val TrackInactive = Color(0xFF1A2538)
private val TrackActiveStart = Color(0xFF2563EB)
private val TrackActiveEnd = Color(0xFF60A5FA)

/**
 * 工控平板风格滑条：圆角轨道、蓝色渐变已选段、白底蓝边拇指带轻阴影。
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NetShieldSlider(
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    onValueChangeFinished: (() -> Unit)? = null,
    enabled: Boolean = true,
) {
    val interactionSource = remember { MutableInteractionSource() }
    val sliderColors = SliderDefaults.colors(
        thumbColor = Color.Transparent,
        activeTrackColor = Color.Transparent,
        inactiveTrackColor = Color.Transparent,
        disabledThumbColor = Color.Transparent,
        disabledActiveTrackColor = Color.Transparent,
        disabledInactiveTrackColor = Color.Transparent,
    )

    Slider(
        value = value,
        onValueChange = onValueChange,
        onValueChangeFinished = onValueChangeFinished,
        enabled = enabled,
        modifier = modifier
            .fillMaxWidth()
            .height(48.dp),
        interactionSource = interactionSource,
        colors = sliderColors,
        thumb = {
            Box(
                modifier = Modifier
                    .size(ThumbSize)
                    .shadow(
                        elevation = 6.dp,
                        shape = CircleShape,
                        ambientColor = Color.Black.copy(alpha = 0.35f),
                        spotColor = Color.Black.copy(alpha = 0.2f),
                    )
                    .background(
                        color = if (enabled) Color.White else NetShieldTextSecondary.copy(alpha = 0.5f),
                        shape = CircleShape,
                    )
                    .border(
                        width = 2.5.dp,
                        color = if (enabled) NetShieldAccentBlue else NetShieldTextSecondary.copy(alpha = 0.4f),
                        shape = CircleShape,
                    ),
            )
        },
        track = { sliderState ->
            val range = sliderState.valueRange.endInclusive - sliderState.valueRange.start
            val fraction = if (range > 0f) {
                ((sliderState.value - sliderState.valueRange.start) / range).coerceIn(0f, 1f)
            } else {
                0f
            }
            val alpha = if (enabled) 1f else 0.45f
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(TrackHeight),
            ) {
                val h = size.height
                val radius = CornerRadius(h / 2f, h / 2f)
                drawRoundRect(
                    color = TrackInactive.copy(alpha = alpha),
                    size = Size(size.width, h),
                    cornerRadius = radius,
                )
                if (fraction > 0f) {
                    drawRoundRect(
                        brush = Brush.horizontalGradient(
                            colors = listOf(
                                TrackActiveStart.copy(alpha = alpha),
                                TrackActiveEnd.copy(alpha = alpha),
                            ),
                            startX = 0f,
                            endX = size.width * fraction,
                        ),
                        size = Size(size.width * fraction, h),
                        cornerRadius = radius,
                    )
                    drawRoundRect(
                        color = Color.White.copy(alpha = 0.12f * alpha),
                        topLeft = Offset(0f, 0f),
                        size = Size(size.width * fraction, h * 0.45f),
                        cornerRadius = radius,
                    )
                }
            }
        },
    )
}
