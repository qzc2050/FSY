package com.raydose.netshield.ui.components



import com.raydose.netshield.model.SlaveProbeUi

import com.raydose.netshield.ui.theme.ScreenSpec

import kotlin.math.max

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



internal fun probeCardSlotSpec(

    slotSize: ProbeCardSlotSize,

    standbyFrosted: Boolean,

    cardWidthDp: Float,

    cardHeightDp: Float,

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

        return ProbeCardSlotSpec(

            nameSp = nameSp,

            alarmSp = alarmSp,

            detailSp = detailSp,

            doseSp = doseSp,

            doseUnitSp = doseUnitSp,

            doseUnitGapDp = doseUnitGapDp,

            doseOffsetDp = doseOffsetDp,

            envSp = spFromHeight(h, ProbeCardMetrics.ENV_H, 14f, ScreenSpec.HOME_CARD_ENV_SP.toFloat()),

            offlineSp = offlineSp,

            cornerDp = cornerDp,

            contentPaddingH = contentPaddingH,

            contentPaddingV = contentPaddingV,

            envPaddingH = envPaddingH,

            envPaddingV = envPaddingV,

            envRowGapDp = envRowGapDp,

            envWeight = 0.26f,

            showEnvBar = false,

            envLayout = EnvLayout.SingleRow,

            envItemsPerRow = 5,

            envItems = emptyList(),

        )

    }



    val envItems = fullEnvItems()

    return when (slotSize) {

        ProbeCardSlotSize.Full -> ProbeCardSlotSpec(

            nameSp = nameSp,

            alarmSp = alarmSp,

            detailSp = detailSp,

            doseSp = doseSp,

            doseUnitSp = doseUnitSp,

            doseUnitGapDp = doseUnitGapDp,

            doseOffsetDp = doseOffsetDp,

            envSp = spFromHeight(h, ProbeCardMetrics.ENV_H, 18f, ScreenSpec.HOME_CARD_ENV_SP.toFloat()),

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

        ProbeCardSlotSize.Half -> {

            val envWeight = 0.28f

            ProbeCardSlotSpec(

                nameSp = nameSp,

                alarmSp = alarmSp,

                detailSp = detailSp,

                doseSp = doseSp,

                doseUnitSp = doseUnitSp,

                doseUnitGapDp = doseUnitGapDp,

                doseOffsetDp = doseOffsetDp,

                envSp = envFontSp(h, envWeight, envRows = 2),

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

                envItemsPerRow = 3,

                envItems = envItems,

            )

        }

        ProbeCardSlotSize.Quarter -> {

            val envWeight = 0.32f

            ProbeCardSlotSpec(

                nameSp = nameSp,

                alarmSp = alarmSp,

                detailSp = detailSp,

                doseSp = doseSp,

                doseUnitSp = doseUnitSp,

                doseUnitGapDp = doseUnitGapDp,

                doseOffsetDp = doseOffsetDp,

                envSp = envFontSp(h, envWeight, envRows = 3),

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

                envItemsPerRow = 2,

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



/** 环境栏字号：按环境区实际高度与行数反推，避免写死过小值 */

private fun envFontSp(cardHeightDp: Float, envWeight: Float, envRows: Int): Int {

    val barHeight = cardHeightDp * envWeight

    val fromBar = barHeight / envRows / 1.75f

    val fromRatio = cardHeightDp * ProbeCardMetrics.ENV_H

    return max(fromBar, fromRatio * 0.88f)

        .coerceIn(16f, ScreenSpec.HOME_CARD_ENV_SP.toFloat())

        .roundToInt()

}



private fun spFromHeight(heightDp: Float, heightRatio: Float, minSp: Float, maxSp: Float): Int =

    (heightDp * heightRatio).coerceIn(minSp, maxSp).roundToInt()



private fun dpFromHeight(heightDp: Float, heightRatio: Float, minDp: Float, maxDp: Float): Int =

    (heightDp * heightRatio).coerceIn(minDp, maxDp).roundToInt()



private fun dpFromWidth(widthDp: Float, widthRatio: Float, minDp: Float, maxDp: Float): Int =

    (widthDp * widthRatio).coerceIn(minDp, maxDp).roundToInt()



private fun fullEnvItems(): List<Pair<String, (SlaveProbeUi) -> String>> = listOf(

    "温度" to { it.temperature },

    "气压" to { it.pressure },

    "湿度" to { it.humidity },

    "CO2" to { it.co2 },

    "PM2.5" to { "${it.pm25}" },

)


