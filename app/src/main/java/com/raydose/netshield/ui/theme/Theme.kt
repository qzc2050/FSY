package com.raydose.netshield.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val NetShieldColorScheme = darkColorScheme(
    primary = NetShieldAccentBlue,
    onPrimary = Color.White,
    background = NetShieldBackgroundTop,
    onBackground = NetShieldTextPrimary,
    surface = NetShieldCardOffline,
    onSurface = NetShieldTextPrimary,
)

@Composable
fun NetShieldTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = NetShieldColorScheme,
        typography = Typography,
        content = content,
    )
}
