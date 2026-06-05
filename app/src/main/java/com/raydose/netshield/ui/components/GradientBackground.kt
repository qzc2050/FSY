package com.raydose.netshield.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import com.raydose.netshield.ui.theme.NetShieldBackgroundBottom
import com.raydose.netshield.ui.theme.NetShieldBackgroundTop

@Composable
fun GradientBackground(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(NetShieldBackgroundTop, NetShieldBackgroundBottom),
                ),
            ),
    )
}
