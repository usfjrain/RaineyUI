package com.rainey.frontend.ui

import android.app.Application
import android.net.Uri
import android.provider.OpenableColumns
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.rainey.frontend.artwork.ArtworkImporter
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.RomFolder
import com.rainey.frontend.data.entity.System
import com.rainey.frontend.data.repository.GameRepository
import com.rainey.frontend.domain.DefaultSystems
import com.rainey.frontend.domain.RomExtensions
import com.rainey.frontend.launcher.EmulatorLauncher
import com.rainey.frontend.launcher.LaunchResult
import com.rainey.frontend.launcher.RaineyGameLauncher
import com.rainey.frontend.scanner.AndroidGameScanner
import com.rainey.frontend.scanner.RomScanner
import com.rainey.frontend.settings.BorderStyles
import com.rainey.frontend.settings.SettingsStore
import com.rainey.frontend.settings.UserSettings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

enum class RaineyScreen { Splash, Home, Library, Detail, Settings }
enum class LibraryMode { Grid, List }

data class RaineyUiState(
    val screen: RaineyScreen = RaineyScreen.Splash,
    val systems: List<System> = emptyList(),
    val allSystems: List<System> = emptyList(),
    val selectedSystemId: String? = null,
    val games: List<Game> = emptyList(),
    val allGames: List<Game> = emptyList(),
    val recentlyPlayed: List<Game> = emptyList(),
    val favoriteIds: Set<Long> = emptySet(),
    val selectedGameId: Long? = null,
    val libraryMode: LibraryMode = LibraryMode.Grid,
    val folders: List<RomFolder> = emptyList(),
    val settings: UserSettings = UserSettings(),
    val status: String? = null,
    val isScanning: Boolean = false
) {
    val selectedSystem: System? = systems.firstOrNull { it.id == selectedSystemId }
    val selectedGame: Game? = allGames.firstOrNull { it.id == selectedGameId }
}

class RaineyViewModel(
    application: Application,
    private val repository: GameRepository,
    private val settingsStore: SettingsStore,
    private val scanner: RomScanner = RomScanner(application),
    private val androidGameScanner: AndroidGameScanner = AndroidGameScanner(application),
    private val artworkImporter: ArtworkImporter = ArtworkImporter(application),
    private val emulatorLauncher: EmulatorLauncher = RaineyGameLauncher(settingsStore)
) : AndroidViewModel(application) {
    private val screen = MutableStateFlow(RaineyScreen.Splash)
    private val selectedSystemId = MutableStateFlow<String?>(null)
    private val selectedGameId = MutableStateFlow<Long?>(null)
    private val libraryMode = MutableStateFlow(LibraryMode.Grid)
    private val status = MutableStateFlow<String?>(null)
    private val isScanning = MutableStateFlow(false)

    private val gamesForSelectedSystem = selectedSystemId.flatMapLatest { systemId ->
        repository.observeGamesForSystem(systemId ?: DefaultSystems.all.first().id)
    }

    val uiState: StateFlow<RaineyUiState> = combine(
        screen,
        repository.observeSystems(),
        selectedSystemId,
        gamesForSelectedSystem,
        repository.observeAllGames(),
        repository.observeRecentlyPlayed(),
        repository.observeFavoriteIds(),
        selectedGameId,
        libraryMode,
        repository.observeFolders(),
        settingsStore.settings,
        status,
        isScanning
    ) { values ->
        @Suppress("UNCHECKED_CAST")
        val allSystems = values[1] as List<System>
        val currentSystem = values[2] as String?
        val settings = values[10] as UserSettings
        val visibleSystems = allSystems.filter { !settings.isSystemHidden(it.id) }.ifEmpty { allSystems }
        val selectedSystem = currentSystem?.takeIf { id -> visibleSystems.any { it.id == id } }
            ?: visibleSystems.firstOrNull()?.id
        RaineyUiState(
            screen = values[0] as RaineyScreen,
            systems = visibleSystems,
            allSystems = allSystems,
            selectedSystemId = selectedSystem,
            games = values[3] as List<Game>,
            allGames = values[4] as List<Game>,
            recentlyPlayed = values[5] as List<Game>,
            favoriteIds = (values[6] as List<Long>).toSet(),
            selectedGameId = values[7] as Long?,
            libraryMode = values[8] as LibraryMode,
            folders = values[9] as List<RomFolder>,
            settings = settings,
            status = values[11] as String?,
            isScanning = values[12] as Boolean
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), RaineyUiState())

    fun bootstrap() {
        viewModelScope.launch {
            repository.ensureDefaultSystems()
            if (selectedSystemId.value == null) {
                selectedSystemId.value = DefaultSystems.all.first().id
            }
            kotlinx.coroutines.delay(1_100)
            if (screen.value == RaineyScreen.Splash) screen.value = RaineyScreen.Home
        }
    }

    fun openHome() {
        screen.value = RaineyScreen.Home
        selectedGameId.value = null
    }

    fun openSettings() {
        screen.value = RaineyScreen.Settings
    }

    fun selectSystem(systemId: String) {
        selectedSystemId.value = systemId
        selectedGameId.value = null
    }

    fun switchSystem(delta: Int) {
        val systems = uiState.value.systems
        if (systems.isEmpty()) return
        val current = systems.indexOfFirst { it.id == uiState.value.selectedSystemId }.coerceAtLeast(0)
        val next = (current + delta).floorMod(systems.size)
        selectedSystemId.value = systems[next].id
    }

    fun openLibrary(systemId: String? = uiState.value.selectedSystemId) {
        val targetSystemId = systemId ?: return
        selectSystem(targetSystemId)
        screen.value = RaineyScreen.Library
    }

    fun openDetail(gameId: Long) {
        uiState.value.allGames.firstOrNull { it.id == gameId }?.let { game ->
            selectedSystemId.value = game.systemId
        }
        selectedGameId.value = gameId
        screen.value = RaineyScreen.Detail
    }

    fun back() {
        screen.value = when (screen.value) {
            RaineyScreen.Detail -> RaineyScreen.Library
            RaineyScreen.Library, RaineyScreen.Settings -> RaineyScreen.Home
            RaineyScreen.Splash, RaineyScreen.Home -> RaineyScreen.Home
        }
    }

    fun toggleLibraryMode() {
        libraryMode.value = if (libraryMode.value == LibraryMode.Grid) LibraryMode.List else LibraryMode.Grid
    }

    fun addRomFolder(systemId: String, uri: Uri) {
        viewModelScope.launch {
            val systemName = uiState.value.systems.firstOrNull { it.id == systemId }?.name ?: systemId
            repository.addRomFolder(
                RomFolder(
                    systemId = systemId,
                    treeUri = uri.toString(),
                    displayName = "$systemName folder"
                )
            )
            status.value = "ROM folder saved for $systemName."
        }
    }

    fun addManualGame(systemId: String, uri: Uri) {
        viewModelScope.launch {
            val systemName = uiState.value.systems.firstOrNull { it.id == systemId }?.name ?: systemId
            val fileName = displayNameFor(uri)
            val extension = fileName.substringAfterLast('.', missingDelimiterValue = "").lowercase()
            val allowedExtensions = RomExtensions.bySystem[systemId].orEmpty()

            if (allowedExtensions.isEmpty() || extension !in allowedExtensions) {
                status.value = "That file type is not supported for $systemName. Expected: ${allowedExtensions.joinToString { ".$it" }}."
                return@launch
            }

            val importResult = repository.insertScannedGames(
                listOf(
                    Game(
                        systemId = systemId,
                        title = titleFromFileName(fileName),
                        fileName = fileName,
                        contentUri = uri.toString(),
                        extension = extension
                    )
                )
            )
            status.value = if (importResult.inserted > 0) {
                "Added ${titleFromFileName(fileName)} to $systemName."
            } else {
                "${titleFromFileName(fileName)} is already in $systemName."
            }
            openLibrary(systemId)
        }
    }

    fun scanGames() {
        viewModelScope.launch {
            isScanning.value = true
            status.value = "Scanning ROM folders..."
            val folders = repository.allRomFolders()
            val (games, report) = scanner.scan(folders)
            val androidGames = androidGameScanner.scanInstalledGames()
            val importResult = repository.insertScannedGames(games + androidGames)
            isScanning.value = false
            status.value = buildScanStatus(
                newItems = importResult.inserted,
                duplicates = importResult.duplicates + report.duplicateFiles,
                removedDuplicates = importResult.removedDuplicates,
                romsFound = report.foundGames,
                androidFound = androidGames.size,
                skippedFolders = report.skippedFolders,
                unsupportedFiles = report.unsupportedFiles,
                zipArchivesWithoutRoms = report.zipArchivesWithoutRoms
            )
        }
    }

    fun scanAndroidGames() {
        viewModelScope.launch {
            isScanning.value = true
            status.value = "Scanning launchable Android apps..."
            val androidGames = androidGameScanner.scanLaunchableApps()
            val importResult = repository.insertScannedGames(androidGames)
            isScanning.value = false
            status.value = "Android scan complete: ${importResult.inserted} new apps, ${importResult.duplicates} already imported."
        }
    }

    fun scanSelectedSystem() {
        val systemId = uiState.value.selectedSystemId ?: return
        val systemName = uiState.value.selectedSystem?.name ?: systemId
        viewModelScope.launch {
            isScanning.value = true
            status.value = "Scanning $systemName..."
            if (systemId == "android") {
                val androidGames = androidGameScanner.scanLaunchableApps()
                val importResult = repository.insertScannedGames(androidGames)
                isScanning.value = false
                status.value = "$systemName scan complete: ${importResult.inserted} new apps, ${importResult.duplicates} already imported."
                return@launch
            }

            val folders = repository.allRomFolders().filter { it.systemId == systemId }
            if (folders.isEmpty()) {
                isScanning.value = false
                status.value = "No folder saved for $systemName. Pick Folder first."
                return@launch
            }
            val (games, report) = scanner.scan(folders)
            val importResult = repository.insertScannedGames(games)
            isScanning.value = false
            status.value = buildScanStatus(
                newItems = importResult.inserted,
                duplicates = importResult.duplicates + report.duplicateFiles,
                removedDuplicates = importResult.removedDuplicates,
                romsFound = report.foundGames,
                androidFound = 0,
                skippedFolders = report.skippedFolders,
                unsupportedFiles = report.unsupportedFiles,
                zipArchivesWithoutRoms = report.zipArchivesWithoutRoms
            )
        }
    }

    fun resetDatabase() {
        viewModelScope.launch {
            repository.resetLibrary()
            status.value = "Library database reset."
        }
    }

    fun showStatus(message: String) {
        status.value = message
    }

    fun importArtworkForSelectedSystem() {
        val systemId = uiState.value.selectedSystemId ?: return
        val systemName = uiState.value.selectedSystem?.name ?: systemId
        viewModelScope.launch {
            isScanning.value = true
            status.value = "Importing box art for $systemName..."
            val games = repository.gamesForSystem(systemId)
            val result = artworkImporter.importBoxArt(games)
            games.filter { it.boxArtUri == null }
                .forEach { game ->
                    val file = getApplication<Application>().filesDir.resolve("artwork/${game.systemId}/${game.id}.png")
                    if (file.exists()) {
                        repository.updateBoxArt(game.id, file.absolutePath)
                    }
                }
            isScanning.value = false
            status.value = "Artwork import complete: ${result.imported}/${result.attempted} covers found for $systemName."
        }
    }

    fun toggleFavorite(gameId: Long) {
        viewModelScope.launch {
            repository.toggleFavorite(gameId)
        }
    }

    fun setScanlines(enabled: Boolean) {
        viewModelScope.launch {
            settingsStore.setScanlinesEnabled(enabled)
        }
    }

    fun setTheme(theme: String) {
        viewModelScope.launch {
            settingsStore.setTheme(theme)
        }
    }

    fun setBorderStyle(style: String) {
        viewModelScope.launch {
            settingsStore.setBorderStyle(style)
            status.value = if (style == BorderStyles.OFF) {
                "Screen borders disabled."
            } else {
                "$style border selected for native games."
            }
        }
    }

    fun setRetroArchEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsStore.setRetroArchEnabled(enabled)
            status.value = if (enabled) "RetroArch launch mode enabled." else "External emulator launch mode enabled."
        }
    }

    fun setNativeEmulationEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsStore.setNativeEmulationEnabled(enabled)
            status.value = if (enabled) "Native core launch mode enabled." else "Native mode disabled. Rainey will try RetroArch or external emulators."
        }
    }

    fun setTouchControlsEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsStore.setTouchControlsEnabled(enabled)
            status.value = if (enabled) "Touch controls enabled for native games." else "Touch controls hidden for native games."
        }
    }

    fun setTouchControlsOpacity(opacity: Float) {
        viewModelScope.launch {
            settingsStore.setTouchControlsOpacity(opacity)
        }
    }

    fun setControllerLayout(layout: String) {
        viewModelScope.launch {
            settingsStore.setControllerLayout(layout)
            status.value = "$layout controller layout selected."
        }
    }

    fun setVideoScale(scale: Int) {
        viewModelScope.launch {
            settingsStore.setVideoScale(scale)
            status.value = if (scale == 0) "Native video scale set to Auto." else "Native video scale set to ${scale}x."
        }
    }

    fun setRetroArchPackageName(packageName: String) {
        viewModelScope.launch {
            settingsStore.setRetroArchPackageName(packageName)
        }
    }

    fun setRetroArchCoresFolder(uri: Uri) {
        viewModelScope.launch {
            settingsStore.setRetroArchCoresFolder(uri.toString())
            status.value = "RetroArch cores folder saved."
        }
    }

    fun setBiosFolder(uri: Uri) {
        viewModelScope.launch {
            settingsStore.setBiosFolder(uri.toString())
            status.value = "BIOS/system folder saved."
        }
    }

    fun setRetroArchCore(systemId: String, coreNameOrPath: String) {
        viewModelScope.launch {
            settingsStore.setRetroArchCore(systemId, coreNameOrPath)
        }
    }

    fun setSystemHidden(systemId: String, hidden: Boolean) {
        viewModelScope.launch {
            val allSystems = uiState.value.allSystems
            val currentHidden = uiState.value.settings.hiddenSystemIds
            val nextHidden = if (hidden) currentHidden + systemId else currentHidden - systemId
            if (hidden && allSystems.isNotEmpty() && allSystems.all { it.id in nextHidden }) {
                status.value = "At least one console must stay visible."
                return@launch
            }

            settingsStore.setSystemHidden(systemId, hidden)
            if (hidden && selectedSystemId.value == systemId) {
                selectedSystemId.value = allSystems.firstOrNull { it.id !in nextHidden }?.id
                selectedGameId.value = null
            }
            val systemName = allSystems.firstOrNull { it.id == systemId }?.name ?: systemId
            status.value = if (hidden) "$systemName hidden from Home." else "$systemName shown on Home."
        }
    }

    fun play(game: Game) {
        viewModelScope.launch {
            status.value = "Preparing ${game.title}..."
            val result = withContext(Dispatchers.IO) {
                emulatorLauncher.launch(getApplication(), game)
            }
            when (result) {
                LaunchResult.Launched -> {
                    repository.markPlayed(game.id)
                    status.value = "Launching ${game.title}."
                }
                is LaunchResult.MissingEmulator -> {
                    status.value = "Missing emulator for ${game.systemId}. Install/configure package: ${result.packageName ?: "none"}."
                }
                is LaunchResult.NotConfigured -> status.value = result.message
                is LaunchResult.Failed -> status.value = result.message
            }
        }
    }

    private fun displayNameFor(uri: Uri): String {
        val resolver = getApplication<Application>().contentResolver
        resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0 && cursor.moveToFirst()) {
                return cursor.getString(index).orEmpty().ifBlank { uri.lastPathSegment ?: "content" }
            }
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "content"
    }

    private fun titleFromFileName(name: String): String {
        return name.substringBeforeLast('.', missingDelimiterValue = name)
            .replace(Regex("\\[[^]]*]"), " ")
            .replace(Regex("\\([^)]*\\)"), " ")
            .replace(Regex("^\\s*\\d{1,4}\\s*[-_. ]\\s*"), " ")
            .replace('_', ' ')
            .replace('-', ' ')
            .replace('.', ' ')
            .replace(Regex("\\s+"), " ")
            .trim()
            .ifBlank { name }
    }

    private fun buildScanStatus(
        newItems: Int,
        duplicates: Int,
        removedDuplicates: Int,
        romsFound: Int,
        androidFound: Int,
        skippedFolders: Int,
        unsupportedFiles: Int,
        zipArchivesWithoutRoms: Int
    ): String {
        val details = mutableListOf<String>()
        details += "$newItems new"
        if (duplicates > 0) details += "$duplicates duplicates"
        if (removedDuplicates > 0) details += "$removedDuplicates extras removed"
        details += "$romsFound ROMs"
        if (androidFound > 0) details += "$androidFound Android apps"
        if (skippedFolders > 0) details += "$skippedFolders folders skipped"
        if (unsupportedFiles > 0) details += "$unsupportedFiles unsupported files"
        if (zipArchivesWithoutRoms > 0) details += "$zipArchivesWithoutRoms zip files without supported ROMs"
        return "Scan complete: ${details.joinToString(", ")}."
    }

    private fun Int.floorMod(size: Int): Int = ((this % size) + size) % size
}

class RaineyViewModelFactory(
    private val application: Application,
    private val repository: GameRepository,
    private val settingsStore: SettingsStore
) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        @Suppress("UNCHECKED_CAST")
        return RaineyViewModel(application, repository, settingsStore) as T
    }
}
