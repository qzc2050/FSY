package com.raydose.netshield.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun ProbePageIndicator(
    pageCount: Int,
    currentPage: Int,
    modifier: Modifier = Modifier,
    dotSize: Dp = 8.dp,
    activeDotSize: Dp = 10.dp,
    dotSpacing: Dp = 10.dp,
) {
    if (pageCount <= 0) return

    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center,
    ) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(dotSpacing, Alignment.CenterHorizontally),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            repeat(pageCount) { index ->
                val selected = index == currentPage
                Box(
                    modifier = Modifier
                        .size(if (selected) activeDotSize else dotSize)
                        .clip(CircleShape)
                        .background(
                            if (selected) {
                                NetShieldTextPrimary
                            } else {
                                NetShieldTextSecondary.copy(alpha = 0.45f)
                            },
                        ),
                )
            }
        }
    }
}
