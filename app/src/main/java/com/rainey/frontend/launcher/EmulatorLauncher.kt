package com.rainey.frontend.launcher

import android.content.Context
import com.rainey.frontend.data.entity.Game

interface EmulatorLauncher {
    suspend fun launch(context: Context, game: Game): LaunchResult
}

sealed interface LaunchResult {
    data object Launched : LaunchResult
    data class NotConfigured(val message: String) : LaunchResult
    data class MissingEmulator(val packageName: String?) : LaunchResult
    data class Failed(val message: String) : LaunchResult
}
