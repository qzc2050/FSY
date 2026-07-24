package com.raydose.raylink.ui.settings

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.NetworkCheck
import androidx.compose.material.icons.outlined.Radar
import androidx.compose.material.icons.outlined.Schedule
import androidx.compose.material.icons.automirrored.outlined.VolumeUp
import androidx.compose.ui.graphics.vector.ImageVector

enum class SettingsTab(
    val label: String,
    val icon: ImageVector,
) {
    DisplaySound("显示与声音", Icons.AutoMirrored.Outlined.VolumeUp),
    Network("网络信息", Icons.Outlined.NetworkCheck),
    Time("时间设置", Icons.Outlined.Schedule),
    Probes("探头管理", Icons.Outlined.Radar),
    About("关于本机", Icons.Outlined.Info),
}
