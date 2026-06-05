package com.raydose.netshield.ui.components

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
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

/** 第一行：NetShield 标题 + 下拉箭头 + WiFi/蓝牙/网络图标 */
@Composable
fun HomeTopBar(
    systemName: String,
    onPullDownClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = ScreenSpec.homeHorizontalPadding, vertical = 20.dp),
    ) {
        Text(
            text = systemName,
            color = NetShieldTextPrimary,
            fontSize = 26.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.align(Alignment.CenterStart),
        )
        PullDownHint(
            onClick = onPullDownClick,
            modifier = Modifier.align(Alignment.TopCenter),
        )
        ConnectivityStatusIconsRow(
            modifier = Modifier.align(Alignment.CenterEnd),
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
            color = NetShieldTextSecondary,
            fontSize = ScreenSpec.HOME_DATE_SP.sp,
            fontWeight = FontWeight.Light,
        )
        Text(
            text = timeText,
            color = NetShieldTextPrimary,
            fontSize = ScreenSpec.HOME_TIME_SP.sp,
            fontWeight = FontWeight.Light,
            modifier = Modifier.padding(top = 2.dp),
        )
    }
}
