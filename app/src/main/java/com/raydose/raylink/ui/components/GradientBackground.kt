package com.raydose.raylink.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import com.raydose.raylink.ui.theme.RaylinkBackgroundBottom
import com.raydose.raylink.ui.theme.RaylinkBackgroundTop

@Composable
fun GradientBackground(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(RaylinkBackgroundTop, RaylinkBackgroundBottom),
                ),
            ),
    )
}
