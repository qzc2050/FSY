package com.raydose.raylink.ui.components

import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.Color
import com.raydose.raylink.data.AppBrightness

/** 在内容之上绘制暗化层，不拦截触摸事件。 */
fun Modifier.appBrightnessDim(sliderFraction: Float): Modifier = drawWithContent {
    drawContent()
    val alpha = AppBrightness.dimOverlayAlpha(sliderFraction)
    if (alpha > 0.005f) {
        drawRect(Color.Black.copy(alpha = alpha))
    }
}
