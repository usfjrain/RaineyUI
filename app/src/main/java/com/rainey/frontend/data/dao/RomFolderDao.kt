package com.rainey.frontend.data.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import com.rainey.frontend.data.entity.RomFolder
import kotlinx.coroutines.flow.Flow

@Dao
interface RomFolderDao {
    @Query("SELECT * FROM rom_folders ORDER BY systemId ASC, addedAt DESC")
    fun observeFolders(): Flow<List<RomFolder>>

    @Query("SELECT * FROM rom_folders WHERE systemId = :systemId ORDER BY addedAt DESC")
    fun observeFoldersForSystem(systemId: String): Flow<List<RomFolder>>

    @Query("SELECT * FROM rom_folders")
    suspend fun getAllFolders(): List<RomFolder>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(folder: RomFolder)

    @Query("DELETE FROM rom_folders WHERE id = :id")
    suspend fun delete(id: Long)
}
