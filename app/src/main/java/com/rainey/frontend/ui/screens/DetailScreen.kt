package com.rainey.frontend.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rainey.frontend.ui.RaineyUiState
import com.rainey.frontend.ui.RaineyViewModel
import com.rainey.frontend.ui.adaptive.LocalRaineyWindowProfile
import com.rainey.frontend.ui.components.ArcadeBackground
import com.rainey.frontend.ui.components.AssetBackedArt
import com.rainey.frontend.ui.components.EmptyState
import com.rainey.frontend.ui.components.NeonButton
import com.rainey.frontend.ui.components.StatusMessage
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.TextMuted

@Composable
fun DetailScreen(state: RaineyUiState, viewModel: RaineyViewModel) {
    val game = state.selectedGame
    ArcadeBackground {
        if (game == null) {
            EmptyState(
                title = "Game not found",
                message = "The selected library item is no longer available.",
                modifier = Modifier.fillMaxSize(),
                action = { NeonButton("Back", requestFocus = true, onClick = { viewModel.back() }) }
            )
            return@ArcadeBackground
        }

        BoxWithConstraints(Modifier.fillMaxSize()) {
            val profile = LocalRaineyWindowProfile.current
            val compact = profile.compact
            AnimatedVisibility(
                visible = true,
                enter = fadeIn(tween(220)) + slideInHorizontally(tween(320)) { it / 8 } + scaleIn(tween(260), initialScale = 0.96f),
                exit = fadeOut(tween(140)) + slideOutHorizontally(tween(180)) { -it / 8 }
            ) {
                Row(
                    modifier = Modifier.fillMaxSize().padding(profile.dp(if (compact) 18 else 38)),
                    horizontalArrangement = Arrangement.spacedBy(profile.dp(if (compact) 20 else 34)),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    AssetBackedArt(
                        game.systemId,
                        game.title,
                        modifier = Modifier.width(profile.dp(if (compact) 220 else 270)).height(profile.dp(if (compact) 310 else 380)),
                        focused = true,
                        boxArtUri = game.boxArtUri
                    )
                    Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(profile.dp(if (compact) 8 else 14))) {
                        Text(
                            game.title,
                            color = Color.White,
                            fontSize = profile.sp(if (compact) 28 else 40),
                            fontWeight = FontWeight.Black,
                            fontFamily = FontFamily.Monospace,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis
                        )
                        Text(state.selectedSystem?.name ?: game.systemId, color = NeonBlue, fontSize = profile.sp(if (compact) 15 else 18), fontFamily = FontFamily.Monospace)
                        Row(horizontalArrangement = Arrangement.spacedBy(profile.dp(if (compact) 8 else 12)), modifier = Modifier.fillMaxWidth()) {
                            NeonButton("Play", requestFocus = true, onClick = { viewModel.play(game) })
                            NeonButton(
                                if (state.favoriteIds.contains(game.id)) "Favorited" else "Favorite",
                                selected = state.favoriteIds.contains(game.id),
                                onClick = { viewModel.toggleFavorite(game.id) }
                            )
                            NeonButton("Back", onClick = { viewModel.back() })
                        }
                        Spacer(Modifier.height(profile.dp(if (compact) 2 else 8)))
                        Metadata("Year", game.year ?: "Unknown", compact)
                        Metadata("Genre", game.genre ?: "Unknown", compact)
                        Metadata("Developer", game.developer ?: "Unknown", compact)
                        Text("Description", color = TextMuted, fontSize = if (compact) 12.sp else 14.sp, fontFamily = FontFamily.Monospace)
                        Text(
                            game.description ?: "Metadata placeholder. Add local editing or artwork import later.",
                            color = Color.White,
                            fontSize = if (compact) 13.sp else 17.sp,
                            fontFamily = FontFamily.Monospace,
                            lineHeight = if (compact) 18.sp else 24.sp,
                            maxLines = if (compact) 3 else 6,
                            overflow = TextOverflow.Ellipsis
                        )
                        StatusMessage(message = state.status)
                    }
                }
            }
        }
    }
}

@Composable
private fun Metadata(label: String, value: String, compact: Boolean) {
    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("$label:", color = TextMuted, fontSize = if (compact) 13.sp else 16.sp, fontFamily = FontFamily.Monospace)
        Text(value, color = Color.White, fontSize = if (compact) 13.sp else 16.sp, fontFamily = FontFamily.Monospace, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}
