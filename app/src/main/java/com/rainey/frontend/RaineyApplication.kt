package com.rainey.frontend

import android.app.Application
import com.rainey.frontend.data.db.RaineyDatabase
import com.rainey.frontend.data.repository.GameRepository
import com.rainey.frontend.settings.SettingsStore

class RaineyApplication : Application() {
    val database: RaineyDatabase by lazy { RaineyDatabase.create(this) }
    val repository: GameRepository by lazy { GameRepository(database) }
    val settingsStore: SettingsStore by lazy { SettingsStore(this) }
}
