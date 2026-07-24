package com.raydose.raylink.ui.components

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.ScreenSpec

/** 第一行：Raylink 标题 + 连通状态图标（下拉箭头由 [StatusBarWithGesture] 浮层提供） */
@Composable
fun HomeTopBar(
    systemName: String,
    bluetoothOnline: Boolean = false,
    ethernetOnline: Boolean = false,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = ScreenSpec.homeHorizontalPadding, vertical = 20.dp),
    ) {
        Text(
            text = systemName,
            color = RaylinkTextPrimary,
            fontSize = ScreenSpec.HOME_TOP_BAR_TITLE_SP.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.align(Alignment.CenterStart),
        )
        ConnectivityStatusIconsRow(
            modifier = Modifier.align(Alignment.CenterEnd),
            bluetoothOnline = bluetoothOnline,
            ethernetOnline = ethernetOnline,
            showPullIndicator = false,
        )
    }
}

/** 第二行左侧：日期（含农历）+ 时间 */
@Composable
fun DateTimeColumn(
    dateText: String,
    timeText: String,
    modifier: Modifier = Modifier,
) {
    androidx.compose.foundation.layout.Column(modifier = modifier) {
        Text(
            text = dateText,
            color = RaylinkTextSecondary,
            fontSize = ScreenSpec.HOME_DATE_SP.sp,
            fontWeight = FontWeight.Light,
        )
        Text(
            text = timeText,
            color = RaylinkTextPrimary,
            fontSize = ScreenSpec.HOME_TIME_SP.sp,
            fontWeight = FontWeight.Light,
            modifier = Modifier.padding(top = 2.dp),
        )
    }
}
