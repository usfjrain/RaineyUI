package com.rainey.frontend.data.entity

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "systems")
data class System(
    @PrimaryKey val id: String,
    val name: String,
    val shortName: String,
    val sortOrder: Int,
    val logoPlaceholder: String,
    val backgroundPlaceholder: String
)
