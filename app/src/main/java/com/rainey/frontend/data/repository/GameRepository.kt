package com.rainey.frontend.data.repository

import com.rainey.frontend.data.db.RaineyDatabase
import com.rainey.frontend.data.entity.Favorite
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.PlayHistory
import com.rainey.frontend.data.entity.RomFolder
import com.rainey.frontend.domain.DefaultSystems
import kotlinx.coroutines.flow.Flow

data class LibraryImportResult(
    val attempted: Int,
    val inserted: Int,
    val duplicates: Int,
    val removedDuplicates: Int = 0
)

class GameRepository(private val database: RaineyDatabase) {
    fun observeSystems() = database.systemDao().observeSystems()
    fun observeGamesForSystem(systemId: String) = database.gameDao().observeGamesForSystem(systemId)
    fun observeAllGames(): Flow<List<Game>> = database.gameDao().observeAllGames()
    fun observeGame(id: Long) = database.gameDao().observeGame(id)
    fun observeRecentlyPlayed(limit: Int = 12): Flow<List<Game>> = database.gameDao().observeRecentlyPlayed(limit)
    fun observeFavoriteIds(): Flow<List<Long>> = database.gameDao().observeFavoriteIds()
    fun observeFolders() = database.romFolderDao().observeFolders()
    fun observeFoldersForSystem(systemId: String) = database.romFolderDao().observeFoldersForSystem(systemId)

    suspend fun ensureDefaultSystems() {
        database.systemDao().insertAll(DefaultSystems.all)
    }

    suspend fun addRomFolder(folder: RomFolder) {
        database.romFolderDao().insert(folder)
    }

    suspend fun allRomFolders(): List<RomFolder> = database.romFolderDao().getAllFolders()

    suspend fun insertScannedGames(games: List<Game>): LibraryImportResult {
        val rowIds = database.gameDao().insertGames(games)
        val inserted = rowIds.count { it != -1L }
        val removedDuplicates = removeDuplicateGames()
        return LibraryImportResult(
            attempted = games.size,
            inserted = inserted,
            duplicates = games.size - inserted,
            removedDuplicates = removedDuplicates
        )
    }

    suspend fun gamesForSystem(systemId: String): List<Game> = database.gameDao().gamesForSystem(systemId)

    suspend fun updateBoxArt(gameId: Long, boxArtUri: String) {
        database.gameDao().updateBoxArt(gameId, boxArtUri)
    }

    suspend fun toggleFavorite(gameId: Long) {
        if (database.gameDao().isFavorite(gameId)) {
            database.gameDao().removeFavorite(gameId)
        } else {
            database.gameDao().insertFavorite(Favorite(gameId = gameId))
        }
    }

    suspend fun markPlayed(gameId: Long) {
        database.gameDao().insertPlayHistory(PlayHistory(gameId = gameId))
    }

    suspend fun resetLibrary() {
        database.gameDao().resetLibrary()
    }

    private suspend fun removeDuplicateGames(): Int {
        val gameDao = database.gameDao()
        val favorites = gameDao.favoriteIdsSnapshot().toSet()
        val recentRank = gameDao.recentlyPlayedIdsSnapshot()
            .withIndex()
            .associate { it.value to it.index }

        val duplicateIds = gameDao.allGames()
            .groupBy { duplicateKey(it) }
            .values
            .flatMap { group ->
                if (group.size <= 1) {
                    emptyList()
                } else {
                    val keeper = group.maxWith(duplicateComparator(favorites, recentRank))
                    group.filter { it.id != keeper.id }.map { it.id }
                }
            }

        if (duplicateIds.isNotEmpty()) {
            gameDao.deleteGamesByIds(duplicateIds)
        }
        return duplicateIds.size
    }

    private fun duplicateKey(game: Game): String {
        return "${game.systemId}|${normalizeDuplicateTitle(game.title)}"
    }

    private fun normalizeDuplicateTitle(title: String): String {
        return title
            .lowercase()
            .replace(Regex("\\b\\(?(usa|europe|japan|world|rev\\s*\\d+|v\\d+(\\.\\d+)?)\\)?\\b"), " ")
            .replace(Regex("\\b(track|disc|disk|side)\\s*\\d+\\b"), " ")
            .replace(Regex("[^a-z0-9]+"), " ")
            .replace(Regex("\\s+"), " ")
            .trim()
    }

    private fun duplicateComparator(
        favorites: Set<Long>,
        recentRank: Map<Long, Int>
    ): Comparator<Game> = compareBy<Game> { if (it.id in favorites) 1 else 0 }
        .thenBy { if (it.id in recentRank) 1 else 0 }
        .thenBy { recentRank[it.id]?.let { rank -> Int.MAX_VALUE - rank } ?: 0 }
        .thenBy { extensionPriority(it.extension) }
        .thenBy { if (it.boxArtUri != null) 1 else 0 }
        .thenByDescending { it.contentUri.length }

    private fun extensionPriority(extension: String): Int {
        return when (extension.lowercase()) {
            "cue", "gdi", "chd", "cdi", "iso", "cso" -> 5
            "zip", "7z" -> 4
            "sfc", "smc", "nes", "gb", "gbc", "gba", "z64", "n64", "v64", "gen", "md", "nds" -> 3
            "bin" -> 1
            else -> 2
        }
    }
}
