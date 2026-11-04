package com.rainey.frontend.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
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
import com.rainey.frontend.settings.BorderStyles
import com.rainey.frontend.ui.adaptive.LocalRaineyWindowProfile
import com.rainey.frontend.ui.components.ArcadeBackground
import com.rainey.frontend.ui.components.NeonButton
import com.rainey.frontend.ui.components.StatusMessage
import com.rainey.frontend.ui.theme.CabinetPanel
import com.rainey.frontend.ui.theme.NeonBlue
import com.rainey.frontend.ui.theme.TextMuted

@Composable
fun SettingsScreen(
    state: RaineyUiState,
    viewModel: RaineyViewModel,
    onPickRomFolder: (String) -> Unit,
    onPickCoresFolder: () -> Unit,
    onPickBiosFolder: () -> Unit
) {
    ArcadeBackground {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val profile = LocalRaineyWindowProfile.current
            val compact = profile.compact
            val pagePadding = profile.dp(if (compact) 12 else 30)
            val titleSize = profile.sp(if (compact) 24 else 34)
            val rowHeight = profile.dp(if (compact) 50 else 64)

            LazyColumn(
                verticalArrangement = Arrangement.spacedBy(profile.dp(if (compact) 6 else 10)),
                modifier = Modifier
                    .fillMaxSize()
                    .padding(pagePadding)
            ) {
                item("header") {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(Modifier.weight(1f)) {
                            Text("Settings", color = Color.White, fontSize = titleSize, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace)
                            Text("ROM folders, theme, emulator paths", color = TextMuted, fontSize = if (compact) 12.sp else 15.sp, fontFamily = FontFamily.Monospace)
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(if (compact) 6.dp else 8.dp)) {
                            NeonButton("Scan", selected = state.isScanning, requestFocus = true, onClick = { viewModel.scanGames() })
                            NeonButton("Back", onClick = { viewModel.back() })
                        }
                    }
                }

                item("theme") {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Text("Scanlines", color = Color.White, fontSize = if (compact) 14.sp else 18.sp, fontFamily = FontFamily.Monospace)
                        Switch(checked = state.settings.scanlinesEnabled, onCheckedChange = { viewModel.setScanlines(it) })
                        NeonButton("Neon", selected = state.settings.theme == "Neon Arcade", onClick = { viewModel.setTheme("Neon Arcade") })
                        NeonButton("CRT", selected = state.settings.theme == "Midnight CRT", onClick = { viewModel.setTheme("Midnight CRT") })
                    }
                }

                item("screen-border") {
                    Column(
                        verticalArrangement = Arrangement.spacedBy(if (compact) 7.dp else 10.dp),
                        modifier = Modifier.padding(bottom = if (compact) 4.dp else 8.dp)
                    ) {
                        Text("Screen border", color = Color.White, fontSize = if (compact) 14.sp else 18.sp, fontFamily = FontFamily.Monospace)
                        BorderStyles.options.chunked(if (compact) 2 else 4).forEach { row ->
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                row.forEach { style ->
                                    NeonButton(
                                        text = style,
                                        selected = state.settings.borderStyle == style,
                                        modifier = Modifier.weight(1f),
                                        onClick = { viewModel.setBorderStyle(style) }
                                    )
                                }
                            }
                        }
                        Text(
                            "Borders appear over native games and resize the video into the selected frame.",
                            color = TextMuted,
                            fontSize = if (compact) 10.sp else 12.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                item("libraries-title") {
                    Text("Libraries", color = NeonBlue, fontSize = if (compact) 15.sp else 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                }

                item("retroarch-global") {
                    Column(verticalArrangement = Arrangement.spacedBy(if (compact) 7.dp else 10.dp), modifier = Modifier.padding(bottom = if (compact) 6.dp else 12.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 12.dp)) {
                            Text("Use native libretro cores", color = Color.White, fontSize = if (compact) 14.sp else 18.sp, fontFamily = FontFamily.Monospace)
                            Switch(
                                checked = state.settings.nativeEmulationEnabled,
                                onCheckedChange = { viewModel.setNativeEmulationEnabled(it) }
                            )
                        }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(if (compact) 8.dp else 12.dp)) {
                            Text("Use RetroArch when configured", color = Color.White, fontSize = if (compact) 14.sp else 18.sp, fontFamily = FontFamily.Monospace)
                            Switch(
                                checked = state.settings.retroArchEnabled,
                                onCheckedChange = { viewModel.setRetroArchEnabled(it) }
                            )
                        }
                        SettingsTextField(
                            label = "RetroArch package",
                            value = state.settings.retroArchPackageName,
                            onValueChange = { viewModel.setRetroArchPackageName(it) },
                            modifier = Modifier.fillMaxWidth()
                        )
                        SetupFolderRow(
                            title = "RetroArch cores folder",
                            value = state.settings.retroArchCoresFolderUri,
                            buttonText = "Pick Cores",
                            onClick = onPickCoresFolder
                        )
                        SetupFolderRow(
                            title = "BIOS/system folder",
                            value = state.settings.biosFolderUri,
                            buttonText = "Pick BIOS",
                            onClick = onPickBiosFolder
                        )
                        Text(
                            "Rainey does not bundle RetroArch, cores, BIOS files, or ROMs. Pick folders that contain your legally obtained files.",
                            color = TextMuted,
                            fontSize = if (compact) 10.sp else 12.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                item("controls-title") {
                    Text("Controls", color = NeonBlue, fontSize = if (compact) 15.sp else 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                }

                item("controls-config") {
                    Column(verticalArrangement = Arrangement.spacedBy(if (compact) 7.dp else 10.dp), modifier = Modifier.padding(bottom = if (compact) 6.dp else 12.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                            Text("Touch overlay", color = Color.White, fontSize = if (compact) 14.sp else 18.sp, fontFamily = FontFamily.Monospace)
                            Switch(
                                checked = state.settings.touchControlsEnabled,
                                onCheckedChange = { viewModel.setTouchControlsEnabled(it) }
                            )
                        }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                            Text("Opacity", color = Color.White, fontSize = if (compact) 12.sp else 14.sp, fontFamily = FontFamily.Monospace)
                            Slider(
                                value = state.settings.touchControlsOpacity,
                                onValueChange = { viewModel.setTouchControlsOpacity(it) },
                                valueRange = 0.35f..1f,
                                modifier = Modifier.weight(1f)
                            )
                            Text("${(state.settings.touchControlsOpacity * 100).toInt()}%", color = TextMuted, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
                        }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Text("Button layout", color = Color.White, fontSize = if (compact) 12.sp else 14.sp, fontFamily = FontFamily.Monospace)
                            NeonButton("Nintendo", selected = state.settings.controllerLayout == "Nintendo", onClick = { viewModel.setControllerLayout("Nintendo") })
                            NeonButton("Xbox", selected = state.settings.controllerLayout == "Xbox", onClick = { viewModel.setControllerLayout("Xbox") })
                        }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Text("Video scale", color = Color.White, fontSize = if (compact) 12.sp else 14.sp, fontFamily = FontFamily.Monospace)
                            NeonButton("Auto", selected = state.settings.videoScale == 0, onClick = { viewModel.setVideoScale(0) })
                            (1..4).forEach { scale ->
                                NeonButton("${scale}x", selected = state.settings.videoScale == scale, onClick = { viewModel.setVideoScale(scale) })
                            }
                        }
                        Text(
                            "Native games use this layout for physical gamepads and the touch overlay. Video scale is centered and clamped to the display. Back or Escape exits the emulator.",
                            color = TextMuted,
                            fontSize = if (compact) 10.sp else 12.sp,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }

                item("consoles-title") {
                    Text("Consoles", color = NeonBlue, fontSize = if (compact) 15.sp else 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                }

                items(state.allSystems, key = { it.id }) { system ->
                    val folders = state.folders.filter { it.systemId == system.id }
                    val hidden = state.settings.isSystemHidden(system.id)
                    if (system.id == "android") {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(rowHeight),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(Modifier.weight(1f)) {
                                Text(system.name, color = Color.White, fontSize = if (compact) 15.sp else 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                                Text(
                                    "Scans installed Android apps marked as games",
                                    color = TextMuted,
                                    fontSize = if (compact) 10.sp else 12.sp,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                    fontFamily = FontFamily.Monospace
                                )
                            }
                            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                NeonButton(if (hidden) "Show" else "Hide", selected = hidden, onClick = { viewModel.setSystemHidden(system.id, !hidden) })
                                NeonButton("Scan Apps", onClick = { viewModel.scanAndroidGames() })
                            }
                        }
                    } else {
                        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(rowHeight),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Column(Modifier.weight(1f)) {
                                Text(system.name, color = Color.White, fontSize = if (compact) 15.sp else 18.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                                    Text(
                                        folders.lastOrNull()?.treeUri ?: "No folder selected",
                                        color = TextMuted,
                                    fontSize = if (compact) 10.sp else 12.sp,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        fontFamily = FontFamily.Monospace
                                    )
                                }
                                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    NeonButton(if (hidden) "Show" else "Hide", selected = hidden, onClick = { viewModel.setSystemHidden(system.id, !hidden) })
                                    NeonButton("Pick Folder", onClick = { onPickRomFolder(system.id) })
                                }
                            }
                            SettingsTextField(
                                label = "${system.shortName} RetroArch core",
                                value = state.settings.retroArchCoreFor(system.id),
                                onValueChange = { viewModel.setRetroArchCore(system.id, it) },
                                modifier = Modifier.fillMaxWidth()
                            )
                        }
                    }
                }

                item("footer") {
                    Column(verticalArrangement = Arrangement.spacedBy(6.dp), modifier = Modifier.padding(top = 6.dp, bottom = 28.dp)) {
                        Text("External emulator mappings remain available as fallback when RetroArch is disabled or cannot launch.", color = TextMuted, fontSize = if (compact) 11.sp else 13.sp, fontFamily = FontFamily.Monospace)
                        Text("Controller mapping: placeholder screen planned.", color = TextMuted, fontSize = if (compact) 11.sp else 13.sp, fontFamily = FontFamily.Monospace)
                        Row(horizontalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.padding(top = 6.dp)) {
                            NeonButton("Reset Database", onClick = { viewModel.resetDatabase() })
                            StatusMessage(message = state.status, modifier = Modifier.align(Alignment.CenterVertically))
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SetupFolderRow(
    title: String,
    value: String,
    buttonText: String,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, color = Color.White, fontSize = 15.sp, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
            Text(
                value.ifBlank { "No folder selected" },
                color = TextMuted,
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                fontFamily = FontFamily.Monospace
            )
        }
        NeonButton(buttonText, onClick = onClick)
    }
}

@Composable
private fun SettingsTextField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    TextField(
        value = value,
        onValueChange = onValueChange,
        label = {
            Text(label, fontFamily = FontFamily.Monospace)
        },
        singleLine = true,
        textStyle = androidx.compose.ui.text.TextStyle(
            color = Color.White,
            fontFamily = FontFamily.Monospace,
            fontSize = 14.sp
        ),
        colors = TextFieldDefaults.colors(
            focusedContainerColor = CabinetPanel.copy(alpha = 0.86f),
            unfocusedContainerColor = CabinetPanel.copy(alpha = 0.58f),
            focusedTextColor = Color.White,
            unfocusedTextColor = Color.White,
            focusedLabelColor = NeonBlue,
            unfocusedLabelColor = TextMuted,
            cursorColor = NeonBlue,
            focusedIndicatorColor = NeonBlue,
            unfocusedIndicatorColor = TextMuted.copy(alpha = 0.45f)
        ),
        modifier = modifier
    )
}
