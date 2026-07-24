package com.raydose.raylink.ui.components

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.ui.theme.RaylinkMessageBar
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.ScreenSpec
import kotlinx.coroutines.delay

@Composable
fun MessageTickerBar(
    messageTextAt: (Int) -> String,
    messageCount: Int,
    onClick: () -> Unit,
    onAddMessageClick: () -> Unit,
    modifier: Modifier = Modifier,
    widthFraction: Float = 0.54f,
    animateMessageChange: Boolean = false,
    resetKey: Int = 0,
    onDisplayLineChange: ((messageIndex: Int, lineText: String) -> Unit)? = null,
) {
    val density = LocalDensity.current
    val textViewportHeight = with(density) {
        ScreenSpec.MESSAGE_TICKER_LINE_HEIGHT_SP.sp.toDp()
    }
    var currentMessageIndex by remember { mutableIntStateOf(0) }

    LaunchedEffect(resetKey, messageCount) {
        currentMessageIndex = 0
    }

    Row(
        modifier = modifier
            .fillMaxWidth(widthFraction)
            .clip(RoundedCornerShape(24.dp))
            .background(RaylinkMessageBar)
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            imageVector = Icons.Outlined.Edit,
            contentDescription = "添加留言",
            tint = RaylinkTextPrimary,
            modifier = Modifier
                .size(24.dp)
                .clickable(onClick = onAddMessageClick),
        )
        Box(
            modifier = Modifier
                .weight(1f)
                .height(textViewportHeight)
                .padding(horizontal = 12.dp)
                .clip(RectangleShape),
            contentAlignment = Alignment.CenterStart,
        ) {
            if (animateMessageChange && messageCount > 1) {
                AnimatedContent(
                    targetState = currentMessageIndex,
                    transitionSpec = {
                        slideInVertically(
                            animationSpec = tween(ScreenSpec.MESSAGE_TICKER_SLIDE_MS),
                            initialOffsetY = { height -> height },
                        ) togetherWith slideOutVertically(
                            animationSpec = tween(ScreenSpec.MESSAGE_TICKER_SLIDE_MS),
                            targetOffsetY = { height -> -height },
                        )
                    },
                    label = "messageTickerVerticalScroll",
                ) { index ->
                    MessageTickerLineCycleText(
                        text = messageTextAt(index),
                        repeatCycle = false,
                        onDisplayLineChange = { line ->
                            onDisplayLineChange?.invoke(index, line)
                        },
                        onCycleComplete = {
                            currentMessageIndex = (currentMessageIndex + 1) % messageCount
                        },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            } else {
                val index = currentMessageIndex.coerceIn(0, (messageCount - 1).coerceAtLeast(0))
                MessageTickerLineCycleText(
                    text = messageTextAt(index),
                    repeatCycle = true,
                    onDisplayLineChange = { line ->
                        onDisplayLineChange?.invoke(index, line)
                    },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        }
        Text(text = "$messageCount", color = RaylinkTextSecondary, fontSize = 18.sp)
        Text(text = "  ▲", color = RaylinkTextSecondary, fontSize = 16.sp)
    }
}

/**
 * 单条留言逐行展示：每行按自身填充比例独立分档（3/5/7/10 秒），总时长为各行之和。
 * 回收态视口仅 1 行，按测量结果截取当前行文本直接显示。
 */
@Composable
private fun MessageTickerLineCycleText(
    text: String,
    repeatCycle: Boolean,
    modifier: Modifier = Modifier,
    onCycleComplete: (() -> Unit)? = null,
    onDisplayLineChange: ((lineText: String) -> Unit)? = null,
) {
    val textStyle = TextStyle(
        fontSize = 18.sp,
        lineHeight = ScreenSpec.MESSAGE_TICKER_LINE_HEIGHT_SP.sp,
        color = RaylinkTextPrimary,
    )
    val textMeasurer = rememberTextMeasurer()

    BoxWithConstraints(modifier = modifier) {
        val maxWidthPx = constraints.maxWidth
        val charsPerLine = remember(maxWidthPx) {
            measureMessageTickerCharsPerLine(textMeasurer, textStyle, maxWidthPx)
        }
        val layoutResult = remember(text, maxWidthPx) {
            textMeasurer.measure(
                text = AnnotatedString(text),
                style = textStyle,
                constraints = Constraints(maxWidth = maxWidthPx),
            )
        }
        val lineCount = layoutResult.lineCount.coerceAtLeast(1)
        val lineDwellsMs = remember(text, layoutResult, charsPerLine) {
            messageTickerLineDwellsMs(text, layoutResult, charsPerLine)
        }
        var visibleLineIndex by remember(text) { mutableIntStateOf(0) }

        LaunchedEffect(text, lineDwellsMs, repeatCycle) {
            do {
                for (line in 0 until lineCount) {
                    visibleLineIndex = line
                    delay(lineDwellsMs.getOrElse(line) { ScreenSpec.MESSAGE_TICKER_DWELL_MS_SHORT })
                }
                onCycleComplete?.invoke()
            } while (repeatCycle)
        }

        val displayText = remember(text, visibleLineIndex, layoutResult, lineCount) {
            val line = visibleLineIndex.coerceIn(0, lineCount - 1)
            text.substring(
                layoutResult.getLineStart(line),
                layoutResult.getLineEnd(line),
            ).trimEnd()
        }

        LaunchedEffect(displayText) {
            onDisplayLineChange?.invoke(displayText)
        }

        Text(
            text = displayText,
            style = textStyle,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
