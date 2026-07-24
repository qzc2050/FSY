package com.raydose.raylink.data

import kotlin.math.pow

/**
 * 滑条 0~1 映射为可感知的亮度曲线。
 * 工控机 [android.view.WindowManager.LayoutParams.screenBrightness] 调节范围往往很窄，
 * 叠加软件暗化层后最低/最高差异才明显。
 */
object AppBrightness {
    /** 软件暗化层最大不透明度（滑条=0 时） */
    const val DIM_OVERLAY_MAX = 0.92f

    /** 滑条 0% 对应的有效亮度（原滑条 30% 处），避免误拉到底过暗 */
    private const val SLIDER_MIN_EFFECTIVE = 0.30f

    private const val WINDOW_GAMMA = 2.6
    private const val DIM_GAMMA = 1.15
    private const val WINDOW_FLOOR = 0.002f

    /** 将 UI 滑条 0~1 映射为有效亮度 30%~100% */
    fun effectiveSliderFraction(sliderFraction: Float): Float {
        val t = sliderFraction.coerceIn(0f, 1f)
        return SLIDER_MIN_EFFECTIVE + t * (1f - SLIDER_MIN_EFFECTIVE)
    }

    fun windowLevel(sliderFraction: Float): Float {
        val t = effectiveSliderFraction(sliderFraction)
        if (t <= 0f) return WINDOW_FLOOR
        return WINDOW_FLOOR + t.toDouble().pow(WINDOW_GAMMA).toFloat() * (1f - WINDOW_FLOOR)
    }

    /** 绘制在内容之上的黑色遮罩 alpha，不影响触摸。 */
    fun dimOverlayAlpha(sliderFraction: Float): Float {
        val t = effectiveSliderFraction(sliderFraction)
        if (t >= 1f) return 0f
        return (1f - t.toDouble().pow(DIM_GAMMA).toFloat()) * DIM_OVERLAY_MAX
    }
}
