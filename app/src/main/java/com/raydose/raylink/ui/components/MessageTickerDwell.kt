package com.raydose.raylink.ui.components

import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.Constraints
import com.raydose.raylink.ui.theme.ScreenSpec

/**
 * 按「该行字数 / 单行容量」比例分档停留时长。
 * - &lt; 1/4 → 3s；&lt; 1/2 → 5s；&lt; 3/4 → 7s；≥ 3/4（满行）→ 10s
 */
fun messageTickerDwellMsForFillRatio(fillRatio: Float): Long = when {
    fillRatio < 0.25f -> ScreenSpec.MESSAGE_TICKER_DWELL_MS_SHORT
    fillRatio < 0.50f -> ScreenSpec.MESSAGE_TICKER_DWELL_MS_MEDIUM
    fillRatio < 0.75f -> ScreenSpec.MESSAGE_TICKER_DWELL_MS_LONG
    else -> ScreenSpec.MESSAGE_TICKER_DWELL_MS_FULL
}

/** 单行正文字数相对 [charsPerLine] 的分档停留（毫秒）。 */
fun messageTickerLineDwellMs(lineText: String, charsPerLine: Int): Long {
    val trimmed = lineText.trimEnd()
    if (trimmed.isEmpty()) return ScreenSpec.MESSAGE_TICKER_DWELL_MS_SHORT
    val ratio = trimmed.length.toFloat() / charsPerLine.coerceAtLeast(1)
    return messageTickerDwellMsForFillRatio(ratio)
}

/** 逐行计算停留时长；多行留言总时长 = 各行之和。 */
fun messageTickerLineDwellsMs(
    text: String,
    layoutResult: TextLayoutResult,
    charsPerLine: Int,
): List<Long> = buildList {
    for (line in 0 until layoutResult.lineCount) {
        val lineText = text.substring(
            layoutResult.getLineStart(line),
            layoutResult.getLineEnd(line),
        )
        add(messageTickerLineDwellMs(lineText, charsPerLine))
    }
}

/** 估算留言栏单行可容纳字符数（按中文全角宽度测量）。 */
fun measureMessageTickerCharsPerLine(
    textMeasurer: TextMeasurer,
    style: TextStyle,
    maxWidthPx: Int,
): Int {
    if (maxWidthPx <= 0) return 1
    val probe = "中"
    var lo = 1
    var hi = 512
    while (lo < hi) {
        val mid = (lo + hi + 1) / 2
        val layout = textMeasurer.measure(
            text = AnnotatedString(probe.repeat(mid)),
            style = style,
            constraints = Constraints(maxWidth = maxWidthPx),
        )
        if (layout.lineCount <= 1 && layout.size.width <= maxWidthPx) {
            lo = mid
        } else {
            hi = mid - 1
        }
    }
    return lo.coerceAtLeast(1)
}
