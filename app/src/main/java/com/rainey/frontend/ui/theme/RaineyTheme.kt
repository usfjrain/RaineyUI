package com.rainey.frontend.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val ArcadeBlack = Color(0xFF050812)
val CabinetPanel = Color(0xFF111827)
val NeonBlue = Color(0xFF21D4FD)
val NeonPurple = Color(0xFFB83BFF)
val NeonPink = Color(0xFFFF4FD8)
val TextPrimary = Color(0xFFF4F7FB)
val TextMuted = Color(0xFFA7B0C0)

private val colors = darkColorScheme(
    primary = NeonBlue,
    secondary = NeonPurple,
    tertiary = NeonPink,
    background = ArcadeBlack,
    surface = CabinetPanel,
    onPrimary = Color.Black,
    onSecondary = Color.White,
    onBackground = TextPrimary,
    onSurface = TextPrimary
)

@Composable
fun RaineyTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = colors, content = content)
}
