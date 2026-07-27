package com.raydose.raylink.ui.components

import androidx.annotation.DrawableRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.raydose.raylink.R
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

/**
 * 顶栏右侧：下拉提示 + 蓝牙 / WiFi / 有线网络（与 image9 原型一致）。
 *
 * - 蓝牙：转接板 QCC3084 USB 音频在线
 * - 有线：本机 eth* 已有 IPv4
 * - WiFi：暂不接真实状态，保持常显
 */
@Composable
fun ConnectivityStatusIconsRow(
    modifier: Modifier = Modifier,
    iconSize: Dp = 22.dp,
    tint: Color = RaylinkTextSecondary,
    onlineTint: Color = RaylinkTextPrimary,
    offlineTint: Color = RaylinkTextSecondary.copy(alpha = 0.32f),
    bluetoothOnline: Boolean = false,
    ethernetOnline: Boolean = false,
    showPullIndicator: Boolean = true,
    iconSpacing: Dp = 14.dp,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(iconSpacing),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (showPullIndicator) {
            StatusIcon(R.drawable.ic_status_chevron_up, stringResource(R.string.cd_expand_status_bar), iconSize, tint)
        }
        StatusIcon(
            R.drawable.ic_status_bluetooth,
            stringResource(R.string.status_bluetooth),
            iconSize,
            if (bluetoothOnline) onlineTint else offlineTint,
        )
        StatusIcon(R.drawable.ic_status_wifi, "WiFi", iconSize, tint)
        StatusIcon(
            R.drawable.ic_status_ethernet,
            stringResource(R.string.status_ethernet),
            iconSize,
            if (ethernetOnline) onlineTint else offlineTint,
        )
    }
}

@Composable
private fun StatusIcon(
    @DrawableRes res: Int,
    description: String,
    size: Dp,
    tint: Color,
) {
    Icon(
        painter = painterResource(res),
        contentDescription = description,
        tint = tint,
        modifier = Modifier.size(size),
    )
}
