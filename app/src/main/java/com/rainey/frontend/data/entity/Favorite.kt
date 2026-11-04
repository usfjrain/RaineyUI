package com.rainey.frontend.data.entity

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "favorites",
    foreignKeys = [
        ForeignKey(
            entity = Game::class,
            parentColumns = ["id"],
            childColumns = ["gameId"],
            onDelete = ForeignKey.CASCADE
        )
    ],
    indices = [Index(value = ["gameId"], unique = true)]
)
data class Favorite(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val gameId: Long,
    val addedAt: Long = java.lang.System.currentTimeMillis()
)
