package com.rainey.frontend.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import com.rainey.frontend.ui.adaptive.LocalRaineyWindowProfile
import com.rainey.frontend.ui.adaptive.raineyWindowProfile
import com.rainey.frontend.ui.components.ScanlineOverlay
import com.rainey.frontend.ui.screens.DetailScreen
import com.rainey.frontend.ui.screens.HomeScreen
import com.rainey.frontend.ui.screens.LibraryScreen
import com.rainey.frontend.ui.screens.SettingsScreen
import com.rainey.frontend.ui.screens.SplashScreen

@OptIn(ExperimentalAnimationApi::class)
@Composable
fun RaineyApp(
    viewModel: RaineyViewModel,
    onPickRomFolder: (String) -> Unit,
    onPickCoresFolder: () -> Unit,
    onPickBiosFolder: () -> Unit,
    onLoadContent: (String) -> Unit,
    onImportArtwork: () -> Unit
) {
    val state by viewModel.uiState.collectAsState()
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val windowProfile = raineyWindowProfile(maxWidth, maxHeight)
        CompositionLocalProvider(LocalRaineyWindowProfile provides windowProfile) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .onPreviewKeyEvent { event ->
                        if (event.type != KeyEventType.KeyUp) return@onPreviewKeyEvent false
                        when {
                            event.key == Key.Back || event.key == Key.Escape || event.key == Key.ButtonB -> {
                                viewModel.back()
                                true
                            }
                            event.key == Key.ButtonStart -> {
                                viewModel.openSettings()
                                true
                            }
                            state.screen == RaineyScreen.Home && event.key == Key.ButtonL1 -> {
                                viewModel.switchSystem(-1)
                                true
                            }
                            state.screen == RaineyScreen.Home && event.key == Key.ButtonR1 -> {
                                viewModel.switchSystem(1)
                                true
                            }
                            else -> false
                        }
                    }
            ) {
                AnimatedContent(
                    targetState = state.screen,
                    transitionSpec = { fadeIn(tween(180)) togetherWith fadeOut(tween(160)) },
                    label = "screen-transition"
                ) { screen ->
                    when (screen) {
                        RaineyScreen.Splash -> SplashScreen()
                        RaineyScreen.Home -> HomeScreen(state = state, viewModel = viewModel)
                        RaineyScreen.Library -> LibraryScreen(
                            state = state,
                            viewModel = viewModel,
                            onPickRomFolder = onPickRomFolder,
                            onLoadContent = onLoadContent,
                            onImportArtwork = onImportArtwork
                        )
                        RaineyScreen.Detail -> DetailScreen(state = state, viewModel = viewModel)
                        RaineyScreen.Settings -> SettingsScreen(
                            state = state,
                            viewModel = viewModel,
                            onPickRomFolder = onPickRomFolder,
                            onPickCoresFolder = onPickCoresFolder,
                            onPickBiosFolder = onPickBiosFolder
                        )
                    }
                }
                if (state.settings.scanlinesEnabled) {
                    ScanlineOverlay()
                }
            }
        }
    }
}
