package com.raydose.netshield.ui.theme

import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

val NetShieldBackgroundTop = Color(0xFF1A1B3A)
val NetShieldBackgroundBottom = Color(0xFF2D1F4E)
val NetShieldCardOnlineStart = Color(0xFF6B3FA0)
val NetShieldCardOnlineEnd = Color(0xFF2A3F7A)
val NetShieldCardOffline = Color(0xFF4A4A5A)
val NetShieldAccentBlue = Color(0xFF3B82F6)
val NetShieldTextPrimary = Color(0xFFFFFFFF)
val NetShieldTextSecondary = Color(0xFFB0B8D0)
val NetShieldDoorOpen = Color(0xFFE53935)
val NetShieldDoorClosed = Color(0xFF43A047)
/** 探头卡片辐射报警铃铛 */
val NetShieldAlarmActive = Color(0xFFFF5252)
val NetShieldMessageBar = Color(0xFF1E3A5F)
/** 待机页留言条目：极淡蓝底 */
val NetShieldStandbyMessageBg = Color(0x3358B4F8)

/** 设置页三区背景：顶栏探头摘要 / 左导航 / 中间设备管理 */
val NetShieldSettingsSummaryBg = Color(0xFF1C2E4A)
val NetShieldSettingsNavBg = Color(0xFF0C1018)
val NetShieldSettingsContentBg = Color(0xFF283242)
/** 中间区内探头编辑卡片（略亮于内容区底） */
val NetShieldSettingsEditorPanel = Color(0xFF343F52)

/** 参考音乐/相册原型的氛围底色：顶部冷蓝灰，主体蓝黑到紫红横向渐变。 */
val NetShieldAtmosphereTopBarBg = Color(0xFF303858)
val NetShieldAtmosphereGradientStart = Color(0xFF101426)
val NetShieldAtmosphereGradientMid = Color(0xFF342047)
val NetShieldAtmosphereGradientWarm = Color(0xFF4A2947)
val NetShieldAtmosphereGradientEnd = Color(0xFF222437)
val NetShieldAtmosphereListPanelBg = Color(0xCC101421)
val NetShieldAtmospherePlayerOverlay = Color(0x66404047)
val NetShieldAtmosphereBackgroundBrush = Brush.horizontalGradient(
    colors = listOf(
        NetShieldAtmosphereGradientStart,
        NetShieldAtmosphereGradientMid,
        NetShieldAtmosphereGradientWarm,
        NetShieldAtmosphereGradientEnd,
    ),
)
