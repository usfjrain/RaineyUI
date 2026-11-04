package com.rainey.frontend.data.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import com.rainey.frontend.data.dao.GameDao
import com.rainey.frontend.data.dao.RomFolderDao
import com.rainey.frontend.data.dao.SystemDao
import com.rainey.frontend.data.entity.Favorite
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.PlayHistory
import com.rainey.frontend.data.entity.RomFolder
import com.rainey.frontend.data.entity.System

@Database(
    entities = [Game::class, System::class, RomFolder::class, PlayHistory::class, Favorite::class],
    version = 1,
    exportSchema = false
)
abstract class RaineyDatabase : RoomDatabase() {
    abstract fun systemDao(): SystemDao
    abstract fun gameDao(): GameDao
    abstract fun romFolderDao(): RomFolderDao

    companion object {
        fun create(context: Context): RaineyDatabase {
            return Room.databaseBuilder(
                context.applicationContext,
                RaineyDatabase::class.java,
                "rainey.db"
            ).build()
        }
    }
}
