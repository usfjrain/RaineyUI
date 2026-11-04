package com.rainey.frontend.data.entity

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "play_history",
    foreignKeys = [
        ForeignKey(
            entity = Game::class,
            parentColumns = ["id"],
            childColumns = ["gameId"],
            onDelete = ForeignKey.CASCADE
        )
    ],
    indices = [Index("gameId")]
)
data class PlayHistory(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val gameId: Long,
    val playedAt: Long = java.lang.System.currentTimeMillis()
)
