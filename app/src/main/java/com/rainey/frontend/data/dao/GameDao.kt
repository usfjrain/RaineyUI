package com.rainey.frontend.data.dao

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import com.rainey.frontend.data.entity.Favorite
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.PlayHistory
import kotlinx.coroutines.flow.Flow

@Dao
interface GameDao {
    @Query("SELECT * FROM games WHERE systemId = :systemId ORDER BY title COLLATE NOCASE ASC")
    fun observeGamesForSystem(systemId: String): Flow<List<Game>>

    @Query("SELECT * FROM games ORDER BY systemId ASC, title COLLATE NOCASE ASC")
    fun observeAllGames(): Flow<List<Game>>

    @Query("SELECT * FROM games WHERE id = :id")
    fun observeGame(id: Long): Flow<Game?>

    @Query("SELECT * FROM games WHERE id = :id")
    suspend fun getGame(id: Long): Game?

    @Query("SELECT g.* FROM games g INNER JOIN play_history h ON h.gameId = g.id GROUP BY g.id ORDER BY MAX(h.playedAt) DESC LIMIT :limit")
    fun observeRecentlyPlayed(limit: Int = 12): Flow<List<Game>>

    @Query("SELECT gameId FROM favorites")
    fun observeFavoriteIds(): Flow<List<Long>>

    @Query("SELECT gameId FROM favorites")
    suspend fun favoriteIdsSnapshot(): List<Long>

    @Query("SELECT gameId FROM play_history GROUP BY gameId ORDER BY MAX(playedAt) DESC")
    suspend fun recentlyPlayedIdsSnapshot(): List<Long>

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertGames(games: List<Game>): List<Long>

    @Query("SELECT * FROM games WHERE systemId = :systemId ORDER BY title COLLATE NOCASE ASC")
    suspend fun gamesForSystem(systemId: String): List<Game>

    @Query("SELECT * FROM games ORDER BY systemId ASC, title COLLATE NOCASE ASC")
    suspend fun allGames(): List<Game>

    @Query("DELETE FROM games WHERE id IN (:gameIds)")
    suspend fun deleteGamesByIds(gameIds: List<Long>)

    @Query("UPDATE games SET boxArtUri = :boxArtUri WHERE id = :gameId")
    suspend fun updateBoxArt(gameId: Long, boxArtUri: String)

    @Insert
    suspend fun insertPlayHistory(history: PlayHistory)

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertFavorite(favorite: Favorite)

    @Query("DELETE FROM favorites WHERE gameId = :gameId")
    suspend fun removeFavorite(gameId: Long)

    @Query("SELECT EXISTS(SELECT 1 FROM favorites WHERE gameId = :gameId)")
    suspend fun isFavorite(gameId: Long): Boolean

    @Query("DELETE FROM games")
    suspend fun deleteAllGames()

    @Query("DELETE FROM play_history")
    suspend fun deletePlayHistory()

    @Query("DELETE FROM favorites")
    suspend fun deleteFavorites()

    @Transaction
    suspend fun resetLibrary() {
        deleteFavorites()
        deletePlayHistory()
        deleteAllGames()
    }
}
