package com.rainey.frontend.ui.adaptive

import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

enum class RaineyWindowClass {
    Handheld,
    Compact,
    Medium,
    Expanded
}

data class RaineyWindowProfile(
    val width: Dp,
    val height: Dp,
    val windowClass: RaineyWindowClass,
    val uiScale: Float,
    val fontScale: Float,
    val showSecondaryRails: Boolean
) {
    val compact: Boolean = windowClass == RaineyWindowClass.Handheld || windowClass == RaineyWindowClass.Compact

    fun dp(value: Int): Dp = (value * uiScale).dp
    fun sp(value: Int) = (value * fontScale).sp
}

fun raineyWindowProfile(width: Dp, height: Dp): RaineyWindowProfile {
    val windowClass = when {
        width <= 1000.dp || height <= 700.dp -> RaineyWindowClass.Handheld
        width <= 1280.dp || height <= 820.dp -> RaineyWindowClass.Compact
        width >= 1800.dp && height >= 1000.dp -> RaineyWindowClass.Expanded
        else -> RaineyWindowClass.Medium
    }

    return when (windowClass) {
        RaineyWindowClass.Handheld -> RaineyWindowProfile(
            width = width,
            height = height,
            windowClass = windowClass,
            uiScale = 0.86f,
            fontScale = 0.88f,
            showSecondaryRails = false
        )
        RaineyWindowClass.Compact -> RaineyWindowProfile(
            width = width,
            height = height,
            windowClass = windowClass,
            uiScale = 0.94f,
            fontScale = 0.95f,
            showSecondaryRails = false
        )
        RaineyWindowClass.Medium -> RaineyWindowProfile(
            width = width,
            height = height,
            windowClass = windowClass,
            uiScale = 1f,
            fontScale = 1f,
            showSecondaryRails = true
        )
        RaineyWindowClass.Expanded -> RaineyWindowProfile(
            width = width,
            height = height,
            windowClass = windowClass,
            uiScale = 1.12f,
            fontScale = 1.08f,
            showSecondaryRails = true
        )
    }
}

val LocalRaineyWindowProfile = staticCompositionLocalOf {
    RaineyWindowProfile(
        width = 960.dp,
        height = 648.dp,
        windowClass = RaineyWindowClass.Handheld,
        uiScale = 0.86f,
        fontScale = 0.88f,
        showSecondaryRails = false
    )
}
