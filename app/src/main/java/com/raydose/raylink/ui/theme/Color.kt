package com.raydose.raylink.ui.theme

import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

val RaylinkBackgroundTop = Color(0xFF1A1B3A)
val RaylinkBackgroundBottom = Color(0xFF2D1F4E)
val RaylinkCardOnlineStart = Color(0xFF6B3FA0)
val RaylinkCardOnlineEnd = Color(0xFF2A3F7A)
val RaylinkCardOffline = Color(0xFF4A4A5A)
val RaylinkAccentBlue = Color(0xFF3B82F6)
val RaylinkTextPrimary = Color(0xFFFFFFFF)
val RaylinkTextSecondary = Color(0xFFB0B8D0)
val RaylinkDoorOpen = Color(0xFFE53935)
val RaylinkDoorClosed = Color(0xFF43A047)
/** 探头卡片辐射报警铃铛 */
val RaylinkAlarmActive = Color(0xFFFF5252)
val RaylinkMessageBar = Color(0xFF1E3A5F)
/** 待机页留言条目：极淡蓝底 */
val RaylinkStandbyMessageBg = Color(0x3358B4F8)

/** 设置页三区背景：顶栏探头摘要 / 左导航 / 中间设备管理 */
val RaylinkSettingsSummaryBg = Color(0xFF1C2E4A)
val RaylinkSettingsNavBg = Color(0xFF0C1018)
val RaylinkSettingsContentBg = Color(0xFF283242)
/** 中间区内探头编辑卡片（略亮于内容区底） */
val RaylinkSettingsEditorPanel = Color(0xFF343F52)

/** 参考音乐/相册原型的氛围底色：顶部冷蓝灰，主体蓝黑到紫红横向渐变。 */
val RaylinkAtmosphereTopBarBg = Color(0xFF303858)
val RaylinkAtmosphereGradientStart = Color(0xFF101426)
val RaylinkAtmosphereGradientMid = Color(0xFF342047)
val RaylinkAtmosphereGradientWarm = Color(0xFF4A2947)
val RaylinkAtmosphereGradientEnd = Color(0xFF222437)
val RaylinkAtmosphereListPanelBg = Color(0xCC101421)
val RaylinkAtmospherePlayerOverlay = Color(0x66404047)
val RaylinkAtmosphereBackgroundBrush = Brush.horizontalGradient(
    colors = listOf(
        RaylinkAtmosphereGradientStart,
        RaylinkAtmosphereGradientMid,
        RaylinkAtmosphereGradientWarm,
        RaylinkAtmosphereGradientEnd,
    ),
)
