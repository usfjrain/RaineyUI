package com.rainey.frontend.data.entity

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "games",
    foreignKeys = [
        ForeignKey(
            entity = System::class,
            parentColumns = ["id"],
            childColumns = ["systemId"],
            onDelete = ForeignKey.CASCADE
        )
    ],
    indices = [Index("systemId"), Index(value = ["systemId", "contentUri"], unique = true)]
)
data class Game(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val systemId: String,
    val title: String,
    val fileName: String,
    val contentUri: String,
    val extension: String,
    val boxArtUri: String? = null,
    val year: String? = null,
    val genre: String? = null,
    val developer: String? = null,
    val description: String? = null,
    val lastIndexedAt: Long = java.lang.System.currentTimeMillis()
)
