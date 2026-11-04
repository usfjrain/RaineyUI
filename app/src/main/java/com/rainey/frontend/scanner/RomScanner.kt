package com.rainey.frontend.scanner

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.data.entity.RomFolder
import com.rainey.frontend.domain.RomExtensions
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.zip.ZipInputStream

data class ScanReport(
    val scannedFolders: Int,
    val foundGames: Int,
    val skippedFolders: Int,
    val duplicateFiles: Int,
    val unsupportedFiles: Int,
    val unreadableFiles: Int,
    val zipArchives: Int,
    val zipArchivesWithoutRoms: Int
)

class RomScanner(private val context: Context) {
    suspend fun scan(folders: List<RomFolder>): Pair<List<Game>, ScanReport> = withContext(Dispatchers.IO) {
        val games = mutableListOf<Game>()
        val seen = mutableSetOf<String>()
        val stats = ScanStats(scannedFolders = folders.size)

        folders.forEach { folder ->
            val root = DocumentFile.fromTreeUri(context, Uri.parse(folder.treeUri))
            val allowed = RomExtensions.bySystem[folder.systemId].orEmpty()
            if (root == null || !root.exists() || allowed.isEmpty()) {
                stats.skippedFolders += 1
                return@forEach
            }
            scanDocument(folder.systemId, root, allowed, games, seen, stats)
        }

        games to ScanReport(
            scannedFolders = folders.size,
            foundGames = games.size,
            skippedFolders = stats.skippedFolders,
            duplicateFiles = stats.duplicateFiles,
            unsupportedFiles = stats.unsupportedFiles,
            unreadableFiles = stats.unreadableFiles,
            zipArchives = stats.zipArchives,
            zipArchivesWithoutRoms = stats.zipArchivesWithoutRoms
        )
    }

    private fun scanDocument(
        systemId: String,
        file: DocumentFile,
        allowedExtensions: Set<String>,
        output: MutableList<Game>,
        seen: MutableSet<String>,
        stats: ScanStats
    ) {
        if (file.isDirectory) {
            file.listFiles().forEach { child -> scanDocument(systemId, child, allowedExtensions, output, seen, stats) }
            return
        }

        val name = file.name ?: return
        if (name.startsWith("._") || name == ".DS_Store") {
            stats.unsupportedFiles += 1
            return
        }
        val extension = name.substringAfterLast('.', missingDelimiterValue = "").lowercase()
        if (extension !in allowedExtensions) {
            if (extension.isNotBlank()) stats.unsupportedFiles += 1
            return
        }

        if (systemId == "playstation" && extension == "bin" && hasSiblingCue(file, name)) {
            stats.duplicateFiles += 1
            return
        }

        val scanKey = "$systemId|${file.uri}"
        if (!seen.add(scanKey)) {
            stats.duplicateFiles += 1
            return
        }

        val romInfo = if (extension == "zip" && systemId != "arcade") {
            stats.zipArchives += 1
            inspectCartridgeZip(systemId, file) ?: run {
                stats.zipArchivesWithoutRoms += 1
                return
            }
        } else {
            RomInfo(fileName = name, title = titleFromFileName(name), extension = extension)
        }

        output += Game(
            systemId = systemId,
            title = romInfo.title,
            fileName = name,
            contentUri = file.uri.toString(),
            extension = romInfo.extension
        )
    }

    private fun inspectCartridgeZip(systemId: String, file: DocumentFile): RomInfo? {
        val allowed = cartridgeZipExtensions[systemId].orEmpty()
        if (allowed.isEmpty()) return null

        return try {
            context.contentResolver.openInputStream(file.uri)?.use { input ->
                ZipInputStream(input).use { zip ->
                    while (true) {
                        val entry = zip.nextEntry ?: break
                        if (entry.isDirectory) continue
                        val entryName = entry.name.substringAfterLast('/')
                        val extension = entryName.substringAfterLast('.', missingDelimiterValue = "").lowercase()
                        if (extension in allowed) {
                            return RomInfo(
                                fileName = entryName,
                                title = titleFromFileName(entryName),
                                extension = "zip"
                            )
                        }
                    }
                }
            }
            null
        } catch (_: Exception) {
            null
        }
    }

    private fun hasSiblingCue(file: DocumentFile, name: String): Boolean {
        val parent = file.parentFile ?: return false
        val expected = "${name.substringBeforeLast('.', missingDelimiterValue = name)}.cue"
        return parent.listFiles().any { sibling ->
            val siblingName = sibling.name ?: return@any false
            sibling.isFile &&
                !siblingName.startsWith("._") &&
                siblingName.equals(expected, ignoreCase = true)
        }
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

    private data class RomInfo(
        val fileName: String,
        val title: String,
        val extension: String
    )

    private data class ScanStats(
        val scannedFolders: Int,
        var skippedFolders: Int = 0,
        var duplicateFiles: Int = 0,
        var unsupportedFiles: Int = 0,
        var unreadableFiles: Int = 0,
        var zipArchives: Int = 0,
        var zipArchivesWithoutRoms: Int = 0
    )

    companion object {
        private val cartridgeZipExtensions = mapOf(
            "nes" to setOf("nes"),
            "snes" to setOf("sfc", "smc"),
            "genesis" to setOf("gen", "md", "bin"),
            "gb" to setOf("gb", "gbc"),
            "gba" to setOf("gba"),
            "n64" to setOf("z64", "n64", "v64"),
            "ds" to setOf("nds")
        )
    }
}
