package com.raydose.raylink.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val RaylinkColorScheme = darkColorScheme(
    primary = RaylinkAccentBlue,
    onPrimary = Color.White,
    background = RaylinkBackgroundTop,
    onBackground = RaylinkTextPrimary,
    surface = RaylinkCardOffline,
    onSurface = RaylinkTextPrimary,
)

@Composable
fun RaylinkTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = RaylinkColorScheme,
        typography = Typography,
        content = content,
    )
}
