package com.raydose.netshield.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.theme.NetShieldCardOffline
import com.raydose.netshield.ui.theme.NetShieldCardOnlineEnd
import com.raydose.netshield.ui.theme.NetShieldCardOnlineStart
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

/** 第三行：探头卡片 */
@Composable
fun SlaveProbeCard(
    probe: SlaveProbeUi,
    onDetailClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val cardBrush = if (probe.isOnline) {
        Brush.horizontalGradient(listOf(NetShieldCardOnlineStart, NetShieldCardOnlineEnd))
    } else {
        Brush.horizontalGradient(listOf(NetShieldCardOffline, NetShieldCardOffline.copy(alpha = 0.85f)))
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .clip(RoundedCornerShape(24.dp))
            .background(cardBrush),
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.74f)
                .padding(horizontal = 20.dp, vertical = 16.dp),
        ) {
            // 右上角：报警提示
            Text(
                text = "🔔",
                fontSize = 26.sp,
                color = if (probe.hasAlarm) NetShieldTextPrimary else NetShieldTextSecondary.copy(alpha = 0.45f),
                modifier = Modifier.align(Alignment.TopEnd),
            )

            // 顶部居中：名称 + 搜索按钮
            Row(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                Text(
                    text = probe.name,
                    color = NetShieldTextPrimary,
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Medium,
                )
                Text(
                    text = "🔍",
                    fontSize = 24.sp,
                    modifier = Modifier
                        .padding(start = 10.dp)
                        .clickable(onClick = onDetailClick),
                )
            }

            // 中间：超大辐射量 + 单位（单位相对数字垂直居中，对齐整行高度中点）
            Row(
                modifier = Modifier
                    .align(Alignment.Center)
                    .offset(y = ScreenSpec.homeDoseOffsetY),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                Text(
                    text = probe.doseRateText,
                    color = NetShieldTextPrimary,
                    fontSize = ScreenSpec.HOME_DOSE_SP.sp,
                    fontWeight = FontWeight.Light,
                    lineHeight = ScreenSpec.HOME_DOSE_SP.sp,
                )
                Text(
                    text = probe.doseUnit,
                    color = NetShieldTextSecondary,
                    fontSize = ScreenSpec.HOME_DOSE_UNIT_SP.sp,
                    modifier = Modifier.padding(start = ScreenSpec.homeDoseUnitGap),
                )
            }
        }

        // 底部：探头环境参数（一行 5 项，含 PM2.5）
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .weight(0.26f)
                .clip(
                    RoundedCornerShape(
                        topStart = 0.dp,
                        topEnd = 0.dp,
                        bottomEnd = 24.dp,
                        bottomStart = 24.dp,
                    ),
                )
                .background(Color.Black.copy(alpha = 0.25f))
                .padding(horizontal = 8.dp, vertical = 12.dp),
            contentAlignment = Alignment.Center,
        ) {
            if (probe.isOnline) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceEvenly,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    EnvChip("温度", probe.temperature)
                    EnvChip("气压", probe.pressure)
                    EnvChip("湿度", probe.humidity)
                    EnvChip("CO2", probe.co2)
                    EnvChip("PM2.5", "${probe.pm25} ug/m3")
                }
            } else {
                Text(text = "...", color = NetShieldTextSecondary, fontSize = 28.sp)
            }
        }
    }
}

@Composable
private fun EnvChip(label: String, value: String) {
    Row(verticalAlignment = Alignment.Bottom) {
        Text(
            text = "$label:",
            color = NetShieldTextSecondary,
            fontSize = ScreenSpec.HOME_CARD_ENV_SP.sp,
        )
        Text(
            text = " $value",
            color = NetShieldTextPrimary,
            fontSize = ScreenSpec.HOME_CARD_ENV_SP.sp,
            fontWeight = FontWeight.Medium,
        )
    }
}
