package com.rainey.frontend.launcher

import android.content.Context
import android.util.Log
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.settings.SettingsStore
import com.rainey.frontend.settings.UserSettings

class RaineyGameLauncher(
    private val settingsStore: SettingsStore,
    private val nativeLauncher: NativeEmulatorLauncher = NativeEmulatorLauncher(),
    private val duckStationLauncher: DuckStationLauncher = DuckStationLauncher(),
    private val dreamcastLauncher: DreamcastLauncher = DreamcastLauncher(),
    private val retroArchLauncher: RetroArchLauncher = RetroArchLauncher(),
    private val externalLauncher: EmulatorLauncher = ExternalIntentLauncher()
) : EmulatorLauncher {
    override suspend fun launch(context: Context, game: Game): LaunchResult {
        val settings = settingsStore.currentSettings()
        val forceNativeFirst = game.systemId in nativeFirstSystems
        Log.i(TAG, "Launch requested title=${game.title} system=${game.systemId} native=${settings.nativeEmulationEnabled} retroarch=${settings.retroArchEnabled} forceNativeFirst=$forceNativeFirst")

        if (game.systemId == "playstation") {
            return launchPlayStation(context, game, settings)
        }
        if (game.systemId == "dreamcast") {
            return launchDreamcast(context, game, settings)
        }
        if (game.systemId == "ds") {
            return when (val externalResult = externalLauncher.launch(context, game)) {
                LaunchResult.Launched -> {
                    Log.i(TAG, "Nintendo DS external emulator launched title=${game.title}")
                    LaunchResult.Launched
                }
                else -> {
                    Log.i(TAG, "Nintendo DS external emulator failed title=${game.title}: $externalResult")
                    externalResult
                }
            }
        }

        if (!settings.nativeEmulationEnabled && !forceNativeFirst) {
            if (settings.retroArchEnabled) {
                return when (val retroArchResult = retroArchLauncher.launch(context, game, settings)) {
                    LaunchResult.Launched -> LaunchResult.Launched
                    else -> externalLauncher.launch(context, game)
                }
            }
            return externalLauncher.launch(context, game)
        }

        if (!settings.retroArchEnabled || forceNativeFirst) {
            val nativeResult = nativeLauncher.launch(context, game, settings)
            Log.i(TAG, "Native launch result title=${game.title}: $nativeResult")
            if (nativeResult == LaunchResult.Launched) return LaunchResult.Launched
            if (forceNativeFirst) return nativeResult
            val externalResult = externalLauncher.launch(context, game)
            Log.i(TAG, "External launch result title=${game.title}: $externalResult")
            return externalResult
        }

        when (val nativeResult = nativeLauncher.launch(context, game, settings)) {
            LaunchResult.Launched -> return LaunchResult.Launched
            else -> Log.i(TAG, "Native launch result title=${game.title}: $nativeResult")
        }

        return when (val retroArchResult = retroArchLauncher.launch(context, game, settings)) {
            LaunchResult.Launched -> {
                Log.i(TAG, "RetroArch launched title=${game.title}")
                LaunchResult.Launched
            }
            is LaunchResult.MissingEmulator -> {
                when (val fallback = externalLauncher.launch(context, game)) {
                    LaunchResult.Launched -> LaunchResult.Launched
                    else -> LaunchResult.Failed(
                        "RetroArch package ${retroArchResult.packageName ?: "none"} is not installed. Install RetroArch, update Settings, or configure an external emulator fallback."
                    )
                }
            }
            is LaunchResult.NotConfigured -> {
                when (val fallback = externalLauncher.launch(context, game)) {
                    LaunchResult.Launched -> LaunchResult.Launched
                    else -> LaunchResult.Failed("${retroArchResult.message} External emulator fallback is also not available.")
                }
            }
            is LaunchResult.Failed -> {
                when (val fallback = externalLauncher.launch(context, game)) {
                    LaunchResult.Launched -> LaunchResult.Launched
                    else -> LaunchResult.Failed("${retroArchResult.message} External emulator fallback is also not available.")
                }
            }
        }
    }

    private suspend fun launchPlayStation(
        context: Context,
        game: Game,
        settings: UserSettings
    ): LaunchResult {
        val nativeSettings = settings.copy(
            nativeEmulationEnabled = true,
            retroArchCoreBySystem = settings.retroArchCoreBySystem + (
                "playstation" to normalizedPlayStationCore(settings.retroArchCoreFor("playstation"))
            )
        )
        when (val nativeResult = nativeLauncher.launch(context, game, nativeSettings)) {
            LaunchResult.Launched -> {
                Log.i(TAG, "Native PlayStation launched title=${game.title}")
                return LaunchResult.Launched
            }
            else -> Log.i(TAG, "Native PlayStation failed title=${game.title}: $nativeResult")
        }

        return when (val duckResult = duckStationLauncher.launch(context, game)) {
            LaunchResult.Launched -> {
                Log.i(TAG, "DuckStation launched title=${game.title}")
                LaunchResult.Launched
            }
            is LaunchResult.MissingEmulator -> {
                Log.i(TAG, "DuckStation missing for title=${game.title}: $duckResult")
                launchPlayStationFallback(
                    context = context,
                    game = game,
                    settings = settings,
                    primaryMessage = "DuckStation is not installed. Install DuckStation or configure RetroArch for PlayStation in Settings."
                )
            }
            is LaunchResult.Failed -> {
                Log.i(TAG, "DuckStation failed for title=${game.title}: $duckResult")
                launchPlayStationFallback(
                    context = context,
                    game = game,
                    settings = settings,
                    primaryMessage = "DuckStation could not open this PlayStation game: ${duckResult.message}"
                )
            }
            is LaunchResult.NotConfigured -> {
                launchPlayStationFallback(
                    context = context,
                    game = game,
                    settings = settings,
                    primaryMessage = duckResult.message
                )
            }
        }
    }

    private suspend fun launchPlayStationFallback(
        context: Context,
        game: Game,
        settings: UserSettings,
        primaryMessage: String
    ): LaunchResult {
        val retroArchSettings = settings.copy(
            retroArchEnabled = true,
            retroArchPackageName = settings.retroArchPackageName.ifBlank { "com.retroarch" },
            retroArchCoreBySystem = settings.retroArchCoreBySystem + (
                "playstation" to normalizedPlayStationCore(settings.retroArchCoreFor("playstation"))
            )
        )
        if (context.isPackageInstalled(retroArchSettings.retroArchPackageName) ||
            context.isPackageInstalled("com.retroarch") ||
            context.isPackageInstalled("com.retroarch.aarch64")
        ) {
            when (val retroArchResult = retroArchLauncher.launch(context, game, retroArchSettings)) {
                LaunchResult.Launched -> return LaunchResult.Launched
                is LaunchResult.NotConfigured -> {
                    Log.i(TAG, "PlayStation RetroArch fallback failed title=${game.title}: $retroArchResult")
                    if (retroArchResult.message.contains("Android is blocking RetroArch", ignoreCase = true)) {
                        return LaunchResult.Failed(retroArchResult.message)
                    }
                }
                else -> Log.i(TAG, "PlayStation RetroArch fallback failed title=${game.title}: $retroArchResult")
            }
        }

        return when (val externalResult = externalLauncher.launch(context, game)) {
            LaunchResult.Launched -> LaunchResult.Launched
            else -> {
                Log.i(TAG, "PlayStation external fallback failed title=${game.title}: $externalResult")
                LaunchResult.Failed(primaryMessage)
            }
        }
    }

    private fun normalizedPlayStationCore(coreName: String): String {
        val trimmed = coreName.trim()
        return when {
            trimmed.isBlank() -> "pcsx_rearmed_libretro_android.so"
            trimmed == "pcsx_rearmed_libretro.so" -> "pcsx_rearmed_libretro_android.so"
            else -> trimmed
        }
    }

    private suspend fun launchDreamcast(
        context: Context,
        game: Game,
        settings: UserSettings
    ): LaunchResult {
        return when (val dreamcastResult = dreamcastLauncher.launch(context, game)) {
            LaunchResult.Launched -> {
                Log.i(TAG, "Dreamcast emulator launched title=${game.title}")
                LaunchResult.Launched
            }
            is LaunchResult.MissingEmulator -> {
                Log.i(TAG, "Dreamcast emulator missing for title=${game.title}: $dreamcastResult")
                launchDreamcastFallback(
                    context = context,
                    game = game,
                    settings = settings,
                    primaryMessage = "Flycast or Redream is not installed. Install one of them, then launch the Dreamcast game again."
                )
            }
            is LaunchResult.Failed -> {
                Log.i(TAG, "Dreamcast emulator failed for title=${game.title}: $dreamcastResult")
                launchDreamcastFallback(
                    context = context,
                    game = game,
                    settings = settings,
                    primaryMessage = dreamcastResult.message
                )
            }
            is LaunchResult.NotConfigured -> dreamcastResult
        }
    }

    private suspend fun launchDreamcastFallback(
        context: Context,
        game: Game,
        settings: UserSettings,
        primaryMessage: String
    ): LaunchResult {
        if (settings.retroArchEnabled) {
            when (val retroArchResult = retroArchLauncher.launch(context, game, settings)) {
                LaunchResult.Launched -> return LaunchResult.Launched
                else -> Log.i(TAG, "Dreamcast RetroArch fallback failed title=${game.title}: $retroArchResult")
            }
        }

        return when (val externalResult = externalLauncher.launch(context, game)) {
            LaunchResult.Launched -> LaunchResult.Launched
            else -> {
                Log.i(TAG, "Dreamcast external fallback failed title=${game.title}: $externalResult")
                LaunchResult.Failed(primaryMessage)
            }
        }
    }

    private companion object {
        const val TAG = "RaineyLaunch"
        val nativeFirstSystems = setOf("nes", "snes", "genesis", "gb", "gba", "n64", "playstation")
    }
}
