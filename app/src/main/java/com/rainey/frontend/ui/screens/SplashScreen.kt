package com.rainey.frontend.ui.screens

import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rainey.frontend.R
import com.rainey.frontend.ui.components.ArcadeBackground
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.NeonPurple

@Composable
fun SplashScreen() {
    val transition = rememberInfiniteTransition(label = "boot")
    val alpha by transition.animateFloat(
        initialValue = 0.45f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(620, easing = FastOutSlowInEasing), RepeatMode.Reverse),
        label = "alpha"
    )
    val sweep by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(1200, easing = FastOutSlowInEasing), RepeatMode.Restart),
        label = "boot-sweep"
    )
    ArcadeBackground {
        Image(
            painter = painterResource(R.drawable.boot_screen),
            contentDescription = "Rainey boot screen",
            contentScale = ContentScale.Crop,
            modifier = Modifier
                .fillMaxSize()
                .alpha(0.96f)
        )
        Canvas(Modifier.fillMaxSize()) {
            val y = size.height * (0.26f + sweep * 0.48f)
            drawLine(
                color = NeonBlue.copy(alpha = 0.42f),
                start = Offset(size.width * 0.22f, y),
                end = Offset(size.width * 0.78f, y),
                strokeWidth = 5f
            )
            val barY = size.height * 0.64f
            drawLine(
                color = Color.White.copy(alpha = 0.14f),
                start = Offset(size.width * 0.34f, barY),
                end = Offset(size.width * 0.66f, barY),
                strokeWidth = 8f
            )
            drawLine(
                color = NeonPurple.copy(alpha = 0.82f),
                start = Offset(size.width * 0.34f, barY),
                end = Offset(size.width * (0.34f + sweep * 0.32f), barY),
                strokeWidth = 8f
            )
        }
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.BottomCenter) {
            Text(
                text = "BOOTING ARCADE FRONTEND",
                color = NeonBlue,
                fontSize = 18.sp,
                fontFamily = FontFamily.Monospace,
                modifier = Modifier.alpha(0.58f * alpha).padding(bottom = 34.dp)
            )
        }
    }
}
