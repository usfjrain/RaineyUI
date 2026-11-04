package com.rainey.frontend.data.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import com.rainey.frontend.data.entity.System
import kotlinx.coroutines.flow.Flow

@Dao
interface SystemDao {
    @Query("SELECT * FROM systems ORDER BY sortOrder ASC")
    fun observeSystems(): Flow<List<System>>

    @Query("SELECT * FROM systems WHERE id = :id")
    suspend fun getSystem(id: String): System?

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertAll(systems: List<System>)
}
