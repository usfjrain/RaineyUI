package com.rainey.frontend.ui.screens

import androidx.annotation.DrawableRes
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rainey.frontend.R
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.System
import com.rainey.frontend.ui.RaineyUiState
import com.rainey.frontend.ui.RaineyViewModel
import com.rainey.frontend.ui.adaptive.LocalRaineyWindowProfile
import com.rainey.frontend.ui.components.ArcadeBackground
import com.rainey.frontend.ui.components.AssetBackedArt
import com.rainey.frontend.ui.components.PlaceholderArt
import com.rainey.frontend.ui.components.StatusMessage
import com.rainey.frontend.ui.components.focusGlow
import com.rainey.frontend.ui.components.gamepadActivate
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.NeonPurple
import com.rainey.frontend.ui.theme.CabinetPanel
import com.rainey.frontend.ui.theme.TextMuted

@Composable
fun HomeScreen(state: RaineyUiState, viewModel: RaineyViewModel) {
    val listState = rememberLazyListState()
    val selectedIndex = state.systems.indexOfFirst { it.id == state.selectedSystemId }
    val favoriteGames = remember(state.allGames, state.favoriteIds) {
        state.allGames.filter { it.id in state.favoriteIds }.take(16)
    }
    LaunchedEffect(selectedIndex) {
        if (selectedIndex >= 0) {
            listState.animateScrollToItem(selectedIndex, scrollOffset = -80)
        }
    }
    ArcadeBackground {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val profile = LocalRaineyWindowProfile.current
            val compact = profile.compact
            val horizontalPadding = profile.dp(if (compact) 18 else 36)
            val topPadding = profile.dp(if (compact) 10 else 26)
            val actionWidth = profile.dp(if (compact) 92 else 132)
            val actionHeight = profile.dp(if (compact) 44 else 58)
        Column(Modifier.fillMaxSize().padding(vertical = topPadding)) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(horizontal = horizontalPadding),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Image(
                        painter = painterResource(R.drawable.logo),
                        contentDescription = "Rainey",
                        contentScale = ContentScale.Fit,
                        modifier = Modifier.width(if (compact) 86.dp else 132.dp).height(if (compact) 44.dp else 72.dp)
                    )
                    Text("Select a system", color = TextMuted, fontSize = if (compact) 12.sp else 16.sp, fontFamily = FontFamily.Monospace)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 12.dp)) {
                    ImageActionButton(
                        label = "Library",
                        imageRes = R.drawable.library_button_2,
                        width = actionWidth,
                        height = actionHeight,
                        onClick = { viewModel.openLibrary() }
                    )
                    ImageActionButton(
                        label = "Settings",
                        imageRes = R.drawable.settings_button,
                        width = actionWidth,
                        height = actionHeight,
                        onClick = { viewModel.openSettings() }
                    )
                }
            }
            Spacer(Modifier.height(if (compact) 10.dp else 40.dp))
            LazyRow(
                state = listState,
                contentPadding = PaddingValues(horizontal = horizontalPadding),
                horizontalArrangement = Arrangement.spacedBy(if (compact) 14.dp else 22.dp),
                modifier = Modifier.fillMaxWidth().weight(1f)
            ) {
                items(state.systems, key = { it.id }) { system ->
                    SystemCard(
                        system = system,
                        selected = system.id == state.selectedSystemId,
                        compact = compact,
                        onClick = {
                            viewModel.selectSystem(system.id)
                            viewModel.openLibrary(system.id)
                        },
                        onFocus = { viewModel.selectSystem(system.id) }
                    )
                }
            }
            if (favoriteGames.isNotEmpty()) {
                FavoriteQuickAccessRail(
                    games = favoriteGames,
                    compact = compact,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = horizontalPadding, vertical = if (compact) 4.dp else 8.dp),
                    onGameClick = { viewModel.openDetail(it.id) }
                )
            }
            StatusMessage(
                message = state.status,
                modifier = Modifier.padding(horizontal = horizontalPadding, vertical = 8.dp)
            )
        }
        }
    }
}

@Composable
private fun FavoriteQuickAccessRail(
    games: List<Game>,
    compact: Boolean,
    modifier: Modifier = Modifier,
    onGameClick: (Game) -> Unit
) {
    Column(modifier) {
        Text(
            text = "Favorites",
            color = NeonBlue,
            fontSize = if (compact) 12.sp else 15.sp,
            fontWeight = FontWeight.Black,
            fontFamily = FontFamily.Monospace
        )
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 12.dp),
            contentPadding = PaddingValues(top = if (compact) 5.dp else 8.dp),
            modifier = Modifier
                .fillMaxWidth()
                .height(if (compact) 66.dp else 86.dp)
        ) {
            items(games, key = { "home-favorite-${it.id}" }) { game ->
                FavoriteQuickAccessItem(
                    game = game,
                    compact = compact,
                    onClick = { onGameClick(game) }
                )
            }
        }
    }
}

@Composable
private fun FavoriteQuickAccessItem(
    game: Game,
    compact: Boolean,
    onClick: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .width(if (compact) 174.dp else 226.dp)
            .height(if (compact) 58.dp else 74.dp)
            .focusGlow(active = focused, radius = 18f)
            .clip(RoundedCornerShape(8.dp))
            .background(CabinetPanel.copy(alpha = if (focused) 0.88f else 0.62f))
            .border(
                width = if (focused) 2.dp else 1.dp,
                color = if (focused) NeonBlue else Color.White.copy(alpha = 0.16f),
                shape = RoundedCornerShape(8.dp)
            )
            .padding(if (compact) 6.dp else 8.dp)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick),
        horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AssetBackedArt(
            systemId = game.systemId,
            label = game.title,
            modifier = Modifier
                .width(if (compact) 46.dp else 58.dp)
                .height(if (compact) 46.dp else 58.dp),
            compact = true,
            focused = focused,
            boxArtUri = game.boxArtUri
        )
        Column(Modifier.weight(1f)) {
            Text(
                text = game.title,
                color = Color.White,
                fontSize = if (compact) 11.sp else 13.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = game.systemId.uppercase(),
                color = TextMuted,
                fontSize = if (compact) 8.sp else 10.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                maxLines = 1
            )
        }
    }
}

@Composable
private fun SystemCard(
    system: System,
    selected: Boolean,
    compact: Boolean,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
    onFocus: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    val focusRequester = remember { FocusRequester() }
    val active = focused || selected
    val cardWidth by animateDpAsState(
        if (compact) {
            if (active) 192.dp else 170.dp
        } else {
            if (active) 282.dp else 240.dp
        },
        label = "system-width"
    )
    val cardHeight by animateDpAsState(
        if (compact) {
            if (active) 208.dp else 188.dp
        } else {
            if (active) 338.dp else 300.dp
        },
        label = "system-height"
    )
    val scale by animateFloatAsState(if (active) 1.02f else 0.96f, label = "system-scale")
    LaunchedEffect(focused) {
        if (focused) onFocus()
    }
    LaunchedEffect(selected) {
        if (selected) runCatching { focusRequester.requestFocus() }
    }
    Column(
        modifier = modifier
            .width(cardWidth)
            .height(cardHeight)
            .graphicsLayer {
                scaleX = scale
                scaleY = scale
            }
            .focusGlow(active = active, radius = 26f)
            .clip(RoundedCornerShape(8.dp))
            .background(
                Brush.verticalGradient(
                    listOf(
                        CabinetPanel.copy(alpha = if (active) 0.92f else 0.72f),
                        Color(0xFF080D1D).copy(alpha = 0.92f)
                    )
                )
            )
            .border(
                width = if (active) 3.dp else 1.dp,
                color = if (active) NeonBlue else NeonPurple.copy(alpha = 0.34f),
                shape = RoundedCornerShape(8.dp)
            )
            .padding(if (compact) 10.dp else 14.dp)
            .focusRequester(focusRequester)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick),
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        SystemArtwork(
            system = system,
            focused = active,
            modifier = Modifier.fillMaxWidth().height(
                if (compact) {
                    if (active) 132.dp else 116.dp
                } else {
                    if (active) 212.dp else 184.dp
                }
            )
        )
        Column {
            Text(system.name, color = Color.White, fontSize = if (compact) 17.sp else 25.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
        }
    }
}

@Composable
private fun SystemArtwork(
    system: System,
    focused: Boolean,
    modifier: Modifier = Modifier
) {
    val imageRes = systemImageRes(system.id)
    if (imageRes == null) {
        PlaceholderArt(system.logoPlaceholder, modifier = modifier, focused = focused)
        return
    }

    Box(
        modifier = modifier
            .focusGlow(active = focused, radius = 24f)
            .clip(RoundedCornerShape(8.dp))
            .border(
                width = if (focused) 2.dp else 1.dp,
                color = if (focused) NeonBlue else Color.White.copy(alpha = 0.22f),
                shape = RoundedCornerShape(8.dp)
            ),
        contentAlignment = Alignment.Center
    ) {
        Image(
            painter = painterResource(imageRes),
            contentDescription = "${system.name} artwork",
            contentScale = ContentScale.Crop,
            modifier = Modifier.fillMaxSize()
        )
    }
}

@Composable
private fun ImageActionButton(
    label: String,
    @DrawableRes imageRes: Int,
    width: androidx.compose.ui.unit.Dp = 132.dp,
    height: androidx.compose.ui.unit.Dp = 58.dp,
    onClick: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    Box(
        modifier = Modifier
            .width(width)
            .height(height)
            .focusGlow(active = focused, radius = 18f)
            .clip(RoundedCornerShape(6.dp))
            .background(CabinetPanel.copy(alpha = 0.78f))
            .border(
                width = if (focused) 2.dp else 1.dp,
                color = if (focused) NeonBlue else Color.White.copy(alpha = 0.24f),
                shape = RoundedCornerShape(6.dp)
            )
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick),
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
