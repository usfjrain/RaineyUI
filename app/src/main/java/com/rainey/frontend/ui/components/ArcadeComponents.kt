package com.rainey.frontend.ui.components

import android.graphics.BitmapFactory
import androidx.annotation.DrawableRes
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rainey.frontend.R
import com.rainey.frontend.ui.theme.ArcadeBlack
import com.rainey.frontend.ui.theme.CabinetPanel
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.NeonPurple
import com.rainey.frontend.ui.theme.TextMuted
import com.rainey.frontend.ui.theme.TextPrimary
import java.io.File

fun Modifier.gamepadActivate(onClick: () -> Unit): Modifier = onPreviewKeyEvent { event ->
    if (event.type == KeyEventType.KeyUp && event.key.isConfirmKey()) {
        onClick()
        true
    } else {
        false
    }
}

private fun Key.isConfirmKey(): Boolean {
    return this == Key.Enter ||
        this == Key.NumPadEnter ||
        this == Key.DirectionCenter ||
        this == Key.ButtonA
}

@Composable
fun ArcadeBackground(content: @Composable () -> Unit) {
    val transition = rememberInfiniteTransition(label = "arcade-background")
    val drift by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(9000)),
        label = "grid-drift"
    )
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(ArcadeBlack, Color(0xFF0A1024), Color(0xFF150B24))
                )
            )
    ) {
        Image(
            painter = painterResource(R.drawable.background_2),
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = Modifier
                .fillMaxSize()
                .alpha(0.42f)
        )
        Canvas(Modifier.fillMaxSize()) {
            drawRect(color = Color.Black.copy(alpha = 0.48f))
            val horizon = size.height * 0.68f
            drawRect(
                color = NeonPurple.copy(alpha = 0.10f),
                topLeft = Offset(size.width * (0.05f + drift * 0.04f), 0f),
                size = Size(size.width * 0.22f, size.height)
            )
            drawRect(
                color = NeonBlue.copy(alpha = 0.08f),
                topLeft = Offset(size.width * (0.72f - drift * 0.04f), 0f),
                size = Size(size.width * 0.18f, size.height)
            )
            for (i in 0..9) {
                val y = horizon + ((i * 34f + drift * 34f) % 340f)
                drawLine(
                    color = NeonBlue.copy(alpha = 0.14f),
                    start = Offset(0f, y),
                    end = Offset(size.width, y),
                    strokeWidth = 2f
                )
            }
            for (i in -8..8) {
                val x = size.width / 2f + i * 90f
                drawLine(
                    color = NeonPurple.copy(alpha = 0.16f),
                    start = Offset(size.width / 2f, horizon),
                    end = Offset(x, size.height),
                    strokeWidth = 2f,
                    cap = StrokeCap.Round
                )
            }
        }
        content()
    }
}

@Composable
fun NeonButton(
    text: String,
    modifier: Modifier = Modifier,
    selected: Boolean = false,
    requestFocus: Boolean = false,
    onClick: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    val focusRequester = remember { FocusRequester() }
    val active = focused || selected
    val glow by animateFloatAsState(if (active) 1f else 0f, label = "button-glow")
    val borderColor by animateColorAsState(
        if (active) NeonBlue else NeonPurple.copy(alpha = 0.5f),
        label = "button-border"
    )
    LaunchedEffect(requestFocus) {
        if (requestFocus) runCatching { focusRequester.requestFocus() }
    }
    Box(
        modifier = modifier
            .height(44.dp)
            .clip(RoundedCornerShape(6.dp))
            .focusGlow(active = active, radius = 18f * glow)
            .background(if (active) NeonBlue.copy(alpha = 0.22f) else CabinetPanel.copy(alpha = 0.82f))
            .border(2.dp, borderColor, RoundedCornerShape(6.dp))
            .focusRequester(focusRequester)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = text,
            color = TextPrimary,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
fun PlaceholderArt(
    label: String,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
    focused: Boolean = false
) = GeneratedBoxArt(label = label, modifier = modifier, compact = compact, focused = focused)

@Composable
fun AssetBackedArt(
    systemId: String,
    label: String,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
    focused: Boolean = false,
    boxArtUri: String? = null
) {
    val downloadedArt = remember(boxArtUri) {
        boxArtUri
            ?.let(::File)
            ?.takeIf { it.exists() }
            ?.let { runCatching { BitmapFactory.decodeFile(it.absolutePath)?.asImageBitmap() }.getOrNull() }
    }
    if (downloadedArt != null) {
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(8.dp))
                .focusGlow(active = focused, radius = if (compact) 16f else 24f)
                .border(
                    width = if (focused) 2.dp else 1.dp,
                    color = if (focused) NeonBlue else Color.White.copy(alpha = 0.22f),
                    shape = RoundedCornerShape(8.dp)
                )
                .background(Color.Black),
            contentAlignment = Alignment.Center
        ) {
            Image(
                bitmap = downloadedArt,
                contentDescription = label,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize()
            )
        }
        return
    }

    val imageRes = systemImageRes(systemId)
    if (imageRes == null) {
        GeneratedBoxArt(label = label, modifier = modifier, compact = compact, focused = focused)
        return
    }

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .focusGlow(active = focused, radius = if (compact) 16f else 24f)
            .border(
                width = if (focused) 2.dp else 1.dp,
                color = if (focused) NeonBlue else Color.White.copy(alpha = 0.22f),
                shape = RoundedCornerShape(8.dp)
            ),
        contentAlignment = Alignment.Center
    ) {
        Image(
            painter = painterResource(imageRes),
            contentDescription = label,
            contentScale = ContentScale.Crop,
            modifier = Modifier.fillMaxSize()
        )
    }
}

@Composable
fun GeneratedBoxArt(
    label: String,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
    focused: Boolean = false
) {
    val seed = remember(label) { label.hashCode() }
    val hueA = remember(seed) { seededColor(seed, 0xFF21D4FD) }
    val hueB = remember(seed) { seededColor(seed * 31, 0xFFB83BFF) }
    val glow by animateFloatAsState(if (focused) 1f else 0f, label = "box-art-glow")
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .focusGlow(active = focused, radius = 24f * glow)
            .background(Brush.linearGradient(listOf(hueA, Color(0xFF090E23), hueB)))
            .border(2.dp, if (focused) NeonBlue else NeonBlue.copy(alpha = 0.54f), RoundedCornerShape(8.dp))
            .padding(PaddingValues(10.dp)),
        contentAlignment = Alignment.Center
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val stripeColor = Color.White.copy(alpha = 0.08f)
            val spacing = if (compact) 20f else 34f
            for (i in -10..24) {
                val x = i * spacing
                drawLine(
                    color = stripeColor,
                    start = Offset(x, 0f),
                    end = Offset(x + size.height, size.height),
                    strokeWidth = if (compact) 4f else 7f
                )
            }
            drawRect(
                color = Color.Black.copy(alpha = 0.24f),
                topLeft = Offset(size.width * 0.10f, size.height * 0.11f),
                size = Size(size.width * 0.80f, size.height * 0.78f),
                style = Stroke(width = if (compact) 3f else 6f)
            )
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                text = label.initials(),
                color = Color.White,
                fontSize = if (compact) 20.sp else 46.sp,
                fontWeight = FontWeight.Black,
                fontFamily = FontFamily.Monospace,
                textAlign = TextAlign.Center,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = label.uppercase(),
                color = Color.White.copy(alpha = 0.82f),
                fontSize = if (compact) 10.sp else 16.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                textAlign = TextAlign.Center,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
fun ScanlineOverlay() {
    Canvas(
        modifier = Modifier
            .fillMaxSize()
            .alpha(0.22f)
    ) {
        var y = 0f
        while (y < size.height) {
            drawLine(
                color = Color.Black,
                start = Offset(0f, y),
                end = Offset(size.width, y),
                strokeWidth = 1.5f
            )
            y += 5f
        }
    }
}

@Composable
fun StatusMessage(message: String?, modifier: Modifier = Modifier) {
    if (message == null) return
    val isError = message.contains("not installed", ignoreCase = true) ||
        message.contains("unable", ignoreCase = true) ||
        message.contains("failed", ignoreCase = true)
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(if (isError) Color(0xFF351023).copy(alpha = 0.92f) else CabinetPanel.copy(alpha = 0.92f))
            .border(1.dp, if (isError) Color(0xFFFF6B9A) else NeonBlue.copy(alpha = 0.72f), RoundedCornerShape(6.dp))
            .padding(horizontal = 10.dp, vertical = 7.dp)
    ) {
        Text(
            text = message,
            color = if (isError) Color(0xFFFFC7D8) else TextPrimary,
            fontSize = 12.sp,
            fontFamily = FontFamily.Monospace,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
fun EmptyState(
    title: String,
    message: String,
    modifier: Modifier = Modifier,
    action: (@Composable () -> Unit)? = null
) {
    Column(
        modifier = modifier.fillMaxWidth().padding(14.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .size(64.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(NeonPurple.copy(alpha = 0.12f))
                .border(2.dp, NeonBlue.copy(alpha = 0.55f), RoundedCornerShape(8.dp)),
            contentAlignment = Alignment.Center
        ) {
            Text("R", color = Color.White, fontSize = 32.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace)
        }
        Text(title, color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace, modifier = Modifier.padding(top = 12.dp))
        Text(message, color = TextMuted, fontSize = 13.sp, fontFamily = FontFamily.Monospace, textAlign = TextAlign.Center, modifier = Modifier.padding(top = 5.dp))
        if (action != null) {
            Box(Modifier.padding(top = 12.dp)) { action() }
        }
    }
}

@Composable
fun TopBar(title: String, trailing: @Composable () -> Unit = {}) {
    Row(
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(horizontal = 36.dp, vertical = 22.dp)
    ) {
        Text(
            text = title,
            color = Color.White,
            fontSize = 32.sp,
            fontWeight = FontWeight.Black,
            fontFamily = FontFamily.Monospace
        )
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            trailing()
        }
    }
}

fun Modifier.focusGlow(active: Boolean, radius: Float = 18f): Modifier = drawBehind {
    if (!active) return@drawBehind
    drawRoundRect(
        color = NeonBlue.copy(alpha = 0.20f),
        cornerRadius = androidx.compose.ui.geometry.CornerRadius(radius, radius),
        size = size
    )
    drawRoundRect(
        color = NeonPurple.copy(alpha = 0.16f),
        cornerRadius = androidx.compose.ui.geometry.CornerRadius(radius * 1.2f, radius * 1.2f),
        style = Stroke(width = 8f)
    )
}

fun Modifier.focusScale(active: Boolean): Modifier {
    return graphicsLayer {
        val scale = if (active) 1.055f else 1f
        scaleX = scale
        scaleY = scale
    }
}

private fun seededColor(seed: Int, fallback: Long): Color {
    val safe = if (seed == 0) fallback.toInt() else seed
    val r = 50 + kotlin.math.abs(safe shr 16) % 120
    val g = 80 + kotlin.math.abs(safe shr 8) % 145
    val b = 110 + kotlin.math.abs(safe) % 145
    return Color(r, g, b)
}

private fun String.initials(): String {
    val words = trim().split(Regex("\\s+")).filter { it.isNotBlank() }
    if (words.isEmpty()) return "R"
    return words.take(3).joinToString("") { it.first().uppercase() }.take(3)
}

@DrawableRes
private fun systemImageRes(systemId: String): Int? {
    return when (systemId) {
        "nes" -> R.drawable.nes
        "snes" -> R.drawable.snes
        "genesis" -> R.drawable.sega_genesis
        "playstation" -> R.drawable.ps1
        "psp" -> R.drawable.psp
        "dreamcast" -> R.drawable.sega_dreamcast
        "arcade" -> R.drawable.arcade
        "gb" -> R.drawable.gameboy
        "gba" -> R.drawable.game_boy_advance
        "n64" -> R.drawable.nintendo_64
        "ds" -> R.drawable.nintendo_ds
        "android" -> R.drawable.android
        else -> null
    }
}
