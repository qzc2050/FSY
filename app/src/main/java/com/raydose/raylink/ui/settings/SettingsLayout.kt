package com.raydose.raylink.ui.settings

import androidx.compose.ui.unit.dp

/** 设置页左右栏对齐（导航标题与首行探头摘要共用） */
object SettingsLayout {
    /** 左侧导航栏占屏宽比例 */
    const val NAV_RAIL_WIDTH_FRACTION = 1f / 3f

    val navRailStartPadding = 24.dp
    val navItemInnerPadding = 14.dp
    val navIconSize = 22.dp
    val navIconTextGap = 10.dp

    /** 与导航项（图标+文字）左缘一致，供首行探头摘要对齐 */
    val navContentStart = navRailStartPadding + navItemInnerPadding

    val probeSummaryColumnSpacing = 48.dp
    val probeSummaryNameDoseGap = 18.dp
}
