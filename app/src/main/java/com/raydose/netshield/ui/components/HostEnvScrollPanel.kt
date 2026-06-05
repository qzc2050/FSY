package com.raydose.netshield.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.pager.VerticalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

/**
 * 本机环境参数（转接板 0xEF）。
 *
 * 实现：`VerticalPager` 垂直分页列表，类似 Qt 的 QListWidget 上下滑动；
 * 每页固定显示 [ScreenSpec.HOME_HOST_ENV_ITEMS_PER_PAGE] 条（如 温度+湿度）。
 */
@Composable
fun HostEnvScrollPanel(
    readings: List<Pair<String, String>>,
    modifier: Modifier = Modifier,
) {
    if (readings.isEmpty()) return

    val envFontSize = ScreenSpec.HOME_HOST_ENV_SP.sp
    val lineHeight = (ScreenSpec.HOME_HOST_ENV_SP + 10).sp
    val pages = readings.chunked(ScreenSpec.HOME_HOST_ENV_ITEMS_PER_PAGE)
    val pagerState = rememberPagerState(pageCount = { pages.size.coerceAtLeast(1) })

    VerticalPager(
        state = pagerState,
        modifier = modifier
            .wrapContentWidth(Alignment.End)
            .widthIn(min = 260.dp)
            .height(ScreenSpec.homeHostEnvPanelHeight),
        userScrollEnabled = pages.size > 1,
    ) { page ->
        Column(
            modifier = Modifier.fillMaxSize(),
            horizontalAlignment = Alignment.End,
            verticalArrangement = Arrangement.spacedBy(10.dp, Alignment.CenterVertically),
        ) {
            pages.getOrElse(page) { emptyList() }.forEach { (label, value) ->
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = "$label:",
                        color = NetShieldTextSecondary,
                        fontSize = envFontSize,
                        lineHeight = lineHeight,
                        fontWeight = FontWeight.Light,
                        maxLines = 1,
                        overflow = TextOverflow.Clip,
                    )
                    Text(
                        text = value,
                        color = NetShieldTextPrimary,
                        fontSize = envFontSize,
                        lineHeight = lineHeight,
                        fontWeight = FontWeight.Light,
                        maxLines = 1,
                        overflow = TextOverflow.Clip,
                    )
                }
            }
        }
    }
}
