package com.rainey.frontend.data.entity

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "rom_folders",
    foreignKeys = [
        ForeignKey(
            entity = System::class,
            parentColumns = ["id"],
            childColumns = ["systemId"],
            onDelete = ForeignKey.CASCADE
        )
    ],
    indices = [Index("systemId")]
)
data class RomFolder(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val systemId: String,
    val treeUri: String,
    val displayName: String,
    val addedAt: Long = java.lang.System.currentTimeMillis()
)
