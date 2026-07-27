package com.raydose.raylink.ui.components



import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import com.raydose.raylink.R
import com.raydose.raylink.model.SlaveProbeUi

import com.raydose.raylink.ui.theme.ScreenSpec

import kotlin.math.min
import kotlin.math.roundToInt



enum class ProbeCardSlotSize {

    Full,

    Half,

    Quarter,

}



internal enum class EnvLayout {

    SingleRow,

    MultiRow,

}



internal data class ProbeCardSlotSpec(

    val nameSp: Int,

    val alarmSp: Int,

    val detailSp: Int,

    val doseSp: Int,

    val doseUnitSp: Int,

    val doseUnitGapDp: Int,

    val doseOffsetDp: Int,

    val envSp: Int,
    /** 标签（温度/气压/湿度等）字号，通常小于 [envSp]，避免单行挤掉数值 */
    val envLabelSp: Int,

    val offlineSp: Int,

    val cornerDp: Int,

    val contentPaddingH: Int,

    val contentPaddingV: Int,

    val envPaddingH: Int,

    val envPaddingV: Int,

    val envRowGapDp: Int,

    val envWeight: Float,

    val showEnvBar: Boolean,

    val envLayout: EnvLayout,

    val envItemsPerRow: Int,

    val envItems: List<Pair<String, (SlaveProbeUi) -> String>>,

)



/** 单卡全屏布局基准（T130 横屏：屏宽×[HOME_CARD_WIDTH_FRACTION]、屏高×[HOME_CARD_HEIGHT_FRACTION]） */

private object ProbeCardMetrics {

    const val REF_WIDTH_DP = 1582f

    const val REF_HEIGHT_DP = 538f



    const val NAME_H = 28f / REF_HEIGHT_DP

    const val ALARM_H = 26f / REF_HEIGHT_DP

    const val DETAIL_H = 24f / REF_HEIGHT_DP

    const val DOSE_H = ScreenSpec.HOME_DOSE_SP / REF_HEIGHT_DP

    const val DOSE_UNIT_H = ScreenSpec.HOME_DOSE_UNIT_SP / REF_HEIGHT_DP

    const val DOSE_OFFSET_H = 20f / REF_HEIGHT_DP

    const val ENV_H = ScreenSpec.HOME_CARD_ENV_SP / REF_HEIGHT_DP

    const val OFFLINE_H = 28f / REF_HEIGHT_DP

    const val CORNER_H = 24f / REF_HEIGHT_DP

    const val PAD_V_H = 16f / REF_HEIGHT_DP

    const val PAD_H_W = 20f / REF_WIDTH_DP

    const val DOSE_UNIT_GAP_W = 28f / REF_WIDTH_DP

    const val ENV_PAD_V_H = 6f / REF_HEIGHT_DP

    const val ENV_PAD_H_W = 6f / REF_WIDTH_DP

    const val ENV_ROW_GAP_H = 2f / REF_HEIGHT_DP

}



@Composable
internal fun probeCardSlotSpec(
    slotSize: ProbeCardSlotSize,
    standbyFrosted: Boolean,
    cardWidthDp: Float,
    cardHeightDp: Float,
    standbyWithMessages: Boolean = false,
): ProbeCardSlotSpec {

    val w = cardWidthDp.coerceAtLeast(1f)

    val h = cardHeightDp.coerceAtLeast(1f)



    val nameSp = spFromHeight(h, ProbeCardMetrics.NAME_H, 16f, 28f)

    val alarmSp = spFromHeight(h, ProbeCardMetrics.ALARM_H, 14f, 26f)

    val detailSp = spFromHeight(h, ProbeCardMetrics.DETAIL_H, 14f, 24f)

    val offlineSp = spFromHeight(h, ProbeCardMetrics.OFFLINE_H, 14f, 28f)

    val doseSp = doseFontSp(w, h)

    val doseUnitSp = spFromHeight(h, ProbeCardMetrics.DOSE_UNIT_H, 12f, ScreenSpec.HOME_DOSE_UNIT_SP.toFloat())

    val doseUnitGapDp = dpFromWidth(w, ProbeCardMetrics.DOSE_UNIT_GAP_W, 4f, ScreenSpec.homeDoseUnitGap.value)

    val doseOffsetDp = dpFromHeight(h, ProbeCardMetrics.DOSE_OFFSET_H, 0f, ScreenSpec.homeDoseOffsetY.value)

    val cornerDp = dpFromHeight(h, ProbeCardMetrics.CORNER_H, 10f, 24f)

    val contentPaddingH = dpFromWidth(w, ProbeCardMetrics.PAD_H_W, 4f, 20f)

    val contentPaddingV = dpFromHeight(h, ProbeCardMetrics.PAD_V_H, 2f, 16f)

    val envPaddingH = dpFromWidth(w, ProbeCardMetrics.ENV_PAD_H_W, 4f, 10f)

    val envPaddingV = dpFromHeight(h, ProbeCardMetrics.ENV_PAD_V_H, 2f, 8f)

    val envRowGapDp = dpFromHeight(h, ProbeCardMetrics.ENV_ROW_GAP_H, 1f, 4f)



    if (standbyFrosted) {
        val envItems = fullEnvItems()
        val envValueSp = singleRowEnvFontSp(w, h)
        // 仅「待机 + 留言旁列」卡宽约 2/3 时缩小标签；全宽待机保持标签与数值同号
        val envLabelSp = if (standbyWithMessages) {
            envLabelSpFrom(envValueSp)
        } else {
            envValueSp
        }
        return ProbeCardSlotSpec(
            nameSp = nameSp,
            alarmSp = alarmSp,
            detailSp = detailSp,
            doseSp = doseSp,
            doseUnitSp = doseUnitSp,
            doseUnitGapDp = doseUnitGapDp,
            doseOffsetDp = doseOffsetDp,
            envSp = envValueSp,
            envLabelSp = envLabelSp,
            offlineSp = offlineSp,
            cornerDp = cornerDp,
            contentPaddingH = contentPaddingH,
            contentPaddingV = contentPaddingV,
            envPaddingH = envPaddingH,
            envPaddingV = envPaddingV,
            envRowGapDp = envRowGapDp,
            envWeight = 0.26f,
            showEnvBar = true,
            envLayout = EnvLayout.SingleRow,
            envItemsPerRow = 5,
            envItems = envItems,
        )
    }

    val envItems = fullEnvItems()

    return when (slotSize) {

        ProbeCardSlotSize.Full -> {
            val envValueSp = singleRowEnvFontSp(w, h)
            ProbeCardSlotSpec(
                nameSp = nameSp,
                alarmSp = alarmSp,
                detailSp = detailSp,
                doseSp = doseSp,
                doseUnitSp = doseUnitSp,
                doseUnitGapDp = doseUnitGapDp,
                doseOffsetDp = doseOffsetDp,
                envSp = envValueSp,
                envLabelSp = envValueSp,
                offlineSp = offlineSp,
                cornerDp = cornerDp,
                contentPaddingH = contentPaddingH,
                contentPaddingV = contentPaddingV,
                envPaddingH = envPaddingH,
                envPaddingV = envPaddingV,
                envRowGapDp = envRowGapDp,
                envWeight = 0.26f,
                showEnvBar = true,
                envLayout = EnvLayout.SingleRow,
                envItemsPerRow = 5,
                envItems = envItems,
            )
        }

        ProbeCardSlotSize.Half -> {
            // 2 卡并排时卡宽约一半；每行 3 项在 10 寸上会挤掉湿度等末项，改为 2 列 3 行
            val envWeight = 0.30f
            val envItemsPerRow = 2
            val envRows = 3
            val envValueSp = envFontSp(w, h, envWeight, envRows, envItemsPerRow)
            ProbeCardSlotSpec(
                nameSp = nameSp,
                alarmSp = alarmSp,
                detailSp = detailSp,
                doseSp = doseSp,
                doseUnitSp = doseUnitSp,
                doseUnitGapDp = doseUnitGapDp,
                doseOffsetDp = doseOffsetDp,
                envSp = envValueSp,
                envLabelSp = envValueSp,
                offlineSp = offlineSp,
                cornerDp = cornerDp,
                contentPaddingH = contentPaddingH,
                contentPaddingV = contentPaddingV,
                envPaddingH = envPaddingH,
                envPaddingV = envPaddingV,
                envRowGapDp = envRowGapDp,
                envWeight = envWeight,
                showEnvBar = true,
                envLayout = EnvLayout.MultiRow,
                envItemsPerRow = envItemsPerRow,
                envItems = envItems,
            )
        }

        ProbeCardSlotSize.Quarter -> {
            // 10 寸四卡高度约半格；环境栏需容下 3 行（含 PM2.5 μg/m³），略增高权重
            val envWeight = 0.36f
            val envItemsPerRow = 2
            val envRows = 3
            val envValueSp = envFontSp(w, h, envWeight, envRows, envItemsPerRow)
            ProbeCardSlotSpec(
                nameSp = nameSp,
                alarmSp = alarmSp,
                detailSp = detailSp,
                doseSp = doseSp,
                doseUnitSp = doseUnitSp,
                doseUnitGapDp = doseUnitGapDp,
                doseOffsetDp = doseOffsetDp,
                envSp = envValueSp,
                envLabelSp = envValueSp,
                offlineSp = offlineSp,
                cornerDp = cornerDp,
                contentPaddingH = contentPaddingH,
                contentPaddingV = contentPaddingV,
                envPaddingH = envPaddingH,
                envPaddingV = min(envPaddingV, 3),
                envRowGapDp = min(envRowGapDp, 2),
                envWeight = envWeight,
                showEnvBar = true,
                envLayout = EnvLayout.MultiRow,
                envItemsPerRow = envItemsPerRow,
                envItems = envItems,
            )
        }

    }

}



/** 剂量：同时受卡片高度与宽度约束（2 格宽减半时按宽缩放，4 格宽高均减半时取较小值） */

private fun doseFontSp(cardWidthDp: Float, cardHeightDp: Float): Int {

    val byWidth = ScreenSpec.HOME_DOSE_SP * (cardWidthDp / ProbeCardMetrics.REF_WIDTH_DP)

    val byHeight = cardHeightDp * ProbeCardMetrics.DOSE_H

    return min(byWidth, byHeight)

        .coerceIn(56f, ScreenSpec.HOME_DOSE_SP.toFloat())

        .roundToInt()

}



/**
 * 多行环境栏字号：同时受环境区可用高度与单格宽度约束。
 * 必须按「栏高 / 行数」收敛，不能用整卡比例抬高字号，否则四卡第三行（PM2.5）会被裁切。
 */
private fun envFontSp(
    cardWidthDp: Float,
    cardHeightDp: Float,
    envWeight: Float,
    envRows: Int,
    itemsPerRow: Int,
): Int {
    val rows = envRows.coerceAtLeast(1)
    val barHeight = cardHeightDp * envWeight
    // 预留 padding、行距，以及 μ/g 下降部所需行高（约 1.25× 字号）
    val usable = barHeight * 0.68f
    val byHeight = usable / rows / 1.25f
    val perChipWidth = cardWidthDp / itemsPerRow.coerceAtLeast(1)
    val byWidth = ScreenSpec.HOME_CARD_ENV_SP *
        (perChipWidth / (ProbeCardMetrics.REF_WIDTH_DP / 5f)) *
        ENV_WIDTH_SOFT
    return min(byHeight, byWidth)
        .coerceIn(12f, ScreenSpec.HOME_CARD_ENV_SP.toFloat())
        .roundToInt()
}



/**
 * 单行 5 项环境栏数值字号：按卡高、卡宽收敛。
 * 有留言的窄待机卡宽度已由 [cardWidthDp] 体现，soft 略留余量保证可读。
 */
private const val ENV_WIDTH_SOFT = 1.15f

private fun singleRowEnvFontSp(cardWidthDp: Float, cardHeightDp: Float): Int {
    val byHeight = cardHeightDp * ProbeCardMetrics.ENV_H
    val byWidth = ScreenSpec.HOME_CARD_ENV_SP * (cardWidthDp / ProbeCardMetrics.REF_WIDTH_DP) * ENV_WIDTH_SOFT
    return min(byHeight, byWidth)
        .coerceIn(16f, ScreenSpec.HOME_CARD_ENV_SP.toFloat())
        .roundToInt()
}

/** 有留言的窄待机卡：标签略小于数值（约 85%），避免过小难看 */
private fun envLabelSpFrom(valueSp: Int): Int =
    (valueSp * 0.85f).roundToInt().coerceIn(14, (valueSp - 1).coerceAtLeast(14))



private fun spFromHeight(heightDp: Float, heightRatio: Float, minSp: Float, maxSp: Float): Int =

    (heightDp * heightRatio).coerceIn(minSp, maxSp).roundToInt()



private fun dpFromHeight(heightDp: Float, heightRatio: Float, minDp: Float, maxDp: Float): Int =

    (heightDp * heightRatio).coerceIn(minDp, maxDp).roundToInt()



private fun dpFromWidth(widthDp: Float, widthRatio: Float, minDp: Float, maxDp: Float): Int =

    (widthDp * widthRatio).coerceIn(minDp, maxDp).roundToInt()



@Composable
private fun fullEnvItems(): List<Pair<String, (SlaveProbeUi) -> String>> = listOf(

    stringResource(R.string.env_temperature) to { it.temperature },

    stringResource(R.string.env_pressure) to { it.pressure },

    stringResource(R.string.env_humidity) to { it.humidity },

    "CO2" to { it.co2 },

    "PM2.5" to { "${it.pm25}" },

)


