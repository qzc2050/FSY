package com.raydose.netshield.model

/**
 * 气压显示：界面统一为 kPa，一位小数。
 *
 * - 本机环境等：协议寄存器单位为 Pa。
 * - 探头：3～4 位数值视为 hPa（如 1009 → 100.9 kPa），否则视为 Pa（如 101300 → 101.3 kPa）。
 */
fun formatHostPressureKpa(pressurePa: Long): String =
    "%.1f kPa".format(pressurePa / 1000.0)

fun formatProbePressureKpa(raw: Long): String {
    val kpa = if (isProbePressureLikelyHpa(raw)) raw / 10.0 else raw / 1000.0
    return "%.1f kPa".format(kpa)
}

/** 探头气压：三位或四位整数时按 hPa 处理。 */
private fun isProbePressureLikelyHpa(raw: Long): Boolean {
    if (raw < 0) return false
    return raw.toString().length in 3..4
}
