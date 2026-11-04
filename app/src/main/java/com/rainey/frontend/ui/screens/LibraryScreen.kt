package com.rainey.frontend.ui.screens

import androidx.compose.foundation.border
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.ui.LibraryMode
import com.rainey.frontend.ui.RaineyUiState
import com.rainey.frontend.ui.RaineyViewModel
import com.rainey.frontend.ui.adaptive.LocalRaineyWindowProfile
import com.rainey.frontend.ui.components.ArcadeBackground
import com.rainey.frontend.ui.components.AssetBackedArt
import com.rainey.frontend.ui.components.EmptyState
import com.rainey.frontend.ui.components.NeonButton
import com.rainey.frontend.ui.components.StatusMessage
import com.rainey.frontend.ui.components.focusGlow
import com.rainey.frontend.ui.components.gamepadActivate
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.NeonPink
import com.rainey.frontend.ui.theme.TextMuted

@Composable
fun LibraryScreen(
    state: RaineyUiState,
    viewModel: RaineyViewModel,
    onPickRomFolder: (String) -> Unit,
    onLoadContent: (String) -> Unit,
    onImportArtwork: () -> Unit
) {
    val favoriteGames = state.games.filter { state.favoriteIds.contains(it.id) }
    val recentGames = state.recentlyPlayed.filter { it.systemId == state.selectedSystemId }
    var searchQuery by remember(state.selectedSystemId) { mutableStateOf("") }
    var selectedLetter by remember(state.selectedSystemId) { mutableStateOf<String?>(null) }
    val visibleGames = remember(state.games, searchQuery, selectedLetter) {
        state.games.filter { game ->
            val matchesSearch = searchQuery.isBlank() ||
                game.title.contains(searchQuery.trim(), ignoreCase = true)
            val matchesLetter = selectedLetter == null ||
                game.title.firstSearchLetter() == selectedLetter
            matchesSearch && matchesLetter
        }
    }
    val firstGameId = visibleGames.firstOrNull()?.id
    val activeLetters = remember(state.games) {
        state.games.mapNotNull { it.title.firstSearchLetter() }.toSet()
    }
    ArcadeBackground {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val profile = LocalRaineyWindowProfile.current
            val compact = profile.compact
            val pagePadding = profile.dp(if (compact) 16 else 28)
            val headerTitleSize = profile.sp(if (compact) 26 else 34)
            val gridMinWidth = profile.dp(if (compact) 132 else 170)
            val gridSpacing = profile.dp(if (compact) 10 else 16)
        Column(Modifier.fillMaxSize().padding(pagePadding)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Column {
                    Text(state.selectedSystem?.name ?: "Library", color = Color.White, fontSize = headerTitleSize, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace)
                    Text("${state.games.size} games", color = TextMuted, fontSize = if (compact) 12.sp else 15.sp, fontFamily = FontFamily.Monospace)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(if (compact) 7.dp else 12.dp)) {
                    val selectedSystemId = state.selectedSystemId
                    NeonButton(
                        "Load Content",
                        onClick = {
                            if (selectedSystemId != null) onLoadContent(selectedSystemId)
                        }
                    )
                    NeonButton("Pick Folder", onClick = {
                        if (selectedSystemId != null) onPickRomFolder(selectedSystemId)
                    })
                    NeonButton("Scan", selected = state.isScanning, onClick = { viewModel.scanSelectedSystem() })
                    NeonButton("Import Art", onClick = onImportArtwork)
                    NeonButton(if (state.libraryMode == LibraryMode.Grid) "List" else "Grid", onClick = { viewModel.toggleLibraryMode() })
                    NeonButton("Back", onClick = { viewModel.back() })
                }
            }
            StatusMessage(message = state.status, modifier = Modifier.padding(top = if (compact) 6.dp else 10.dp))
            SearchBox(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                compact = compact,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = if (compact) 8.dp else 12.dp)
            )
            AlphabetIndex(
                activeLetters = activeLetters,
                selectedLetter = selectedLetter,
                compact = compact,
                onSelect = { selectedLetter = it },
                modifier = Modifier.padding(top = if (compact) 6.dp else 8.dp)
            )
            if (favoriteGames.isNotEmpty()) {
                LibraryRail(
                    title = "Favorites",
                    games = favoriteGames,
                    favoriteIds = state.favoriteIds,
                    compact = compact,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = if (compact) 6.dp else 10.dp),
                    onGameClick = { viewModel.openDetail(it.id) }
                )
            }
            Text(
                if (searchQuery.isBlank() && selectedLetter == null) {
                    "All ${state.selectedSystem?.shortName ?: "System"} Games"
                } else {
                    "${visibleGames.size} matches"
                },
                color = NeonBlue,
                fontSize = if (compact) 14.sp else 16.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                modifier = Modifier.padding(top = if (compact) 6.dp else 12.dp)
            )
            if (state.games.isEmpty()) {
                EmptyState(
                    title = "No games scanned yet",
                    message = "Load one ROM here, pick a folder for this console, or scan its saved folders.",
                    modifier = Modifier.weight(1f),
                    action = {
                        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                            val selectedSystemId = state.selectedSystemId
                            NeonButton(
                                "Load Content",
                                requestFocus = true,
                                onClick = {
                                    if (selectedSystemId != null) onLoadContent(selectedSystemId)
                                }
                            )
                            NeonButton("Pick Folder", onClick = {
                                if (selectedSystemId != null) onPickRomFolder(selectedSystemId)
                            })
                            NeonButton("Scan", selected = state.isScanning, onClick = { viewModel.scanSelectedSystem() })
                        }
                    }
                )
            } else if (visibleGames.isEmpty()) {
                EmptyState(
                    title = "No games found",
                    message = "Try a different search or choose All in the A-Z row.",
                    modifier = Modifier.weight(1f),
                    action = {
                        NeonButton(
                            "Clear Search",
                            requestFocus = true,
                            onClick = {
                                searchQuery = ""
                                selectedLetter = null
                            }
                        )
                    }
                )
            } else if (state.libraryMode == LibraryMode.Grid) {
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(gridMinWidth),
                    contentPadding = PaddingValues(top = if (compact) 8.dp else 12.dp),
                    horizontalArrangement = Arrangement.spacedBy(gridSpacing),
                    verticalArrangement = Arrangement.spacedBy(gridSpacing),
                    modifier = Modifier.weight(1f)
                ) {
                    items(visibleGames, key = { it.id }) { game ->
                        GameGridItem(
                            game = game,
                            favorite = state.favoriteIds.contains(game.id),
                            compact = compact,
                            requestFocus = game.id == firstGameId
                        ) { viewModel.openDetail(game.id) }
                    }
                }
            } else {
                LazyColumn(
                    contentPadding = PaddingValues(top = 12.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    items(visibleGames, key = { it.id }) { game ->
                        GameListItem(
                            game = game,
                            favorite = state.favoriteIds.contains(game.id),
                            requestFocus = game.id == firstGameId
                        ) { viewModel.openDetail(game.id) }
                    }
                }
            }
            if (profile.showSecondaryRails && recentGames.isNotEmpty()) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = if (compact) 6.dp else 10.dp),
                    horizontalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    LibraryRail(
                        title = "Recently",
                        games = recentGames,
                        favoriteIds = state.favoriteIds,
                        compact = compact,
                        modifier = Modifier.weight(1f),
                        onGameClick = { viewModel.openDetail(it.id) }
                    )
                }
            }
        }
        }
    }
}

@Composable
private fun SearchBox(
    value: String,
    onValueChange: (String) -> Unit,
    compact: Boolean,
    modifier: Modifier = Modifier
) {
    var focused by remember { mutableStateOf(false) }
    BasicTextField(
        value = value,
        onValueChange = onValueChange,
        singleLine = true,
        textStyle = TextStyle(
            color = Color.White,
            fontSize = if (compact) 13.sp else 16.sp,
            fontFamily = FontFamily.Monospace,
            fontWeight = FontWeight.Bold
        ),
        modifier = modifier
            .height(if (compact) 38.dp else 44.dp)
            .focusGlow(active = focused, radius = 18f)
            .clip(RoundedCornerShape(6.dp))
            .background(Color.Black.copy(alpha = 0.42f))
            .border(2.dp, if (focused) NeonBlue else Color.White.copy(alpha = 0.18f), RoundedCornerShape(6.dp))
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .padding(horizontal = if (compact) 10.dp else 14.dp, vertical = if (compact) 8.dp else 10.dp),
        decorationBox = { innerTextField ->
            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.CenterStart) {
                if (value.isBlank()) {
                    Text(
                        "Search games",
                        color = TextMuted,
                        fontSize = if (compact) 13.sp else 16.sp,
                        fontFamily = FontFamily.Monospace,
                        maxLines = 1
                    )
                }
                innerTextField()
            }
        }
    )
}

@Composable
private fun AlphabetIndex(
    activeLetters: Set<String>,
    selectedLetter: String?,
    compact: Boolean,
    onSelect: (String?) -> Unit,
    modifier: Modifier = Modifier
) {
    val letters = remember { ('A'..'Z').map { it.toString() } }
    LazyRow(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(if (compact) 5.dp else 7.dp)
    ) {
        item("all") {
            LetterChip(
                label = "All",
                selected = selectedLetter == null,
                enabled = true,
                compact = compact,
                onClick = { onSelect(null) }
            )
        }
        items(letters, key = { it }) { letter ->
            LetterChip(
                label = letter,
                selected = selectedLetter == letter,
                enabled = letter in activeLetters,
                compact = compact,
                onClick = { if (letter in activeLetters) onSelect(letter) }
            )
        }
    }
}

@Composable
private fun LetterChip(
    label: String,
    selected: Boolean,
    enabled: Boolean,
    compact: Boolean,
    onClick: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    val active = selected || focused
    Box(
        modifier = Modifier
            .width(if (label == "All") (if (compact) 48.dp else 58.dp) else (if (compact) 32.dp else 36.dp))
            .height(if (compact) 30.dp else 34.dp)
            .clip(RoundedCornerShape(6.dp))
            .focusGlow(active = active, radius = 14f)
            .background(
                when {
                    selected -> NeonBlue.copy(alpha = 0.24f)
                    enabled -> Color.Black.copy(alpha = 0.34f)
                    else -> Color.Black.copy(alpha = 0.18f)
                }
            )
            .border(
                1.dp,
                when {
                    active -> NeonBlue
                    enabled -> Color.White.copy(alpha = 0.16f)
                    else -> Color.White.copy(alpha = 0.06f)
                },
                RoundedCornerShape(6.dp)
            )
            .gamepadActivate { if (enabled) onClick() }
            .onFocusChanged { focused = it.isFocused }
            .focusable(enabled)
            .clickable(enabled = enabled, onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Text(
            label,
            color = when {
                selected -> Color.White
                enabled -> TextMuted
                else -> TextMuted.copy(alpha = 0.35f)
            },
            fontSize = if (compact) 11.sp else 12.sp,
            fontWeight = FontWeight.Black,
            fontFamily = FontFamily.Monospace,
            maxLines = 1
        )
    }
}

@Composable
private fun LibraryRail(
    title: String,
    games: List<Game>,
    favoriteIds: Set<Long>,
    compact: Boolean,
    modifier: Modifier = Modifier,
    onGameClick: (Game) -> Unit
) {
    Column(modifier) {
        Text(title, color = NeonBlue, fontSize = if (compact) 11.sp else 12.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(if (compact) 6.dp else 8.dp),
            contentPadding = PaddingValues(top = 4.dp),
            modifier = Modifier.height(if (compact) 54.dp else 64.dp)
        ) {
            items(games.take(12), key = { "${title}-${it.id}" }) { game ->
                RailGameItem(game, favoriteIds.contains(game.id), compact) { onGameClick(game) }
            }
        }
    }
}

@Composable
private fun GameGridItem(
    game: Game,
    favorite: Boolean,
    compact: Boolean,
    requestFocus: Boolean,
    onClick: () -> Unit
) {
    var focused by remember { mutableStateOf(false) }
    val focusRequester = remember { FocusRequester() }
    LaunchedEffect(requestFocus) {
        if (requestFocus) runCatching { focusRequester.requestFocus() }
    }
    Column(
        modifier = Modifier
            .width(if (compact) 132.dp else 170.dp)
            .focusGlow(active = focused, radius = 20f)
            .border(2.dp, if (focused) NeonBlue else Color.White.copy(alpha = 0.16f), RoundedCornerShape(8.dp))
            .padding(if (compact) 7.dp else 10.dp)
            .focusRequester(focusRequester)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick)
    ) {
        AssetBackedArt(game.systemId, game.title, Modifier.fillMaxWidth().height(if (compact) 150.dp else 205.dp), compact = true, focused = focused, boxArtUri = game.boxArtUri)
        Text(
            text = if (favorite) "★ ${game.title}" else game.title,
            color = if (favorite) NeonPink else Color.White,
            fontSize = if (compact) 11.sp else 14.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = FontFamily.Monospace,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}

@Composable
private fun GameListItem(game: Game, favorite: Boolean, requestFocus: Boolean, onClick: () -> Unit) {
    var focused by remember { mutableStateOf(false) }
    val focusRequester = remember { FocusRequester() }
    LaunchedEffect(requestFocus) {
        if (requestFocus) runCatching { focusRequester.requestFocus() }
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(70.dp)
            .focusGlow(active = focused, radius = 18f)
            .border(2.dp, if (focused) NeonBlue else Color.White.copy(alpha = 0.14f), RoundedCornerShape(6.dp))
            .padding(horizontal = 14.dp)
            .focusRequester(focusRequester)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        AssetBackedArt(game.systemId, game.title, Modifier.width(48.dp).height(48.dp), compact = true, focused = focused, boxArtUri = game.boxArtUri)
        Text(if (favorite) "★ ${game.title}" else game.title, color = Color.White, fontSize = 18.sp, fontFamily = FontFamily.Monospace)
    }
}

@Composable
private fun RailGameItem(game: Game, favorite: Boolean, compact: Boolean, onClick: () -> Unit) {
    var focused by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .width(if (compact) 150.dp else 180.dp)
            .height(if (compact) 48.dp else 58.dp)
            .focusGlow(active = focused, radius = 18f)
            .border(2.dp, if (focused) NeonBlue else Color.White.copy(alpha = 0.14f), RoundedCornerShape(8.dp))
            .padding(6.dp)
            .gamepadActivate(onClick)
            .onFocusChanged { focused = it.isFocused }
            .focusable()
            .clickable(onClick = onClick),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        AssetBackedArt(game.systemId, game.title, Modifier.width(if (compact) 34.dp else 42.dp).height(if (compact) 34.dp else 42.dp), compact = true, focused = focused, boxArtUri = game.boxArtUri)
        Column(Modifier.weight(1f)) {
            Text(
                text = if (favorite) "★ ${game.title}" else game.title,
                color = if (favorite) NeonPink else Color.White,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(game.extension.uppercase(), color = TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
        }
    }
}

private fun String.firstSearchLetter(): String? {
    return firstOrNull { it.isLetterOrDigit() }
        ?.uppercaseChar()
        ?.takeIf { it in 'A'..'Z' }
        ?.toString()
}
