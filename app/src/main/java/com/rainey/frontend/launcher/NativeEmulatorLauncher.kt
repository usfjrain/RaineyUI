package com.rainey.frontend.launcher

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import androidx.documentfile.provider.DocumentFile
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.nativeemu.NativeEmulatorActivity
import com.rainey.frontend.settings.UserSettings
import java.io.File
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class NativeEmulatorLauncher {
    suspend fun launch(context: Context, game: Game, settings: UserSettings): LaunchResult {
        if (game.systemId == "android") {
            return LaunchResult.NotConfigured("Android apps launch directly.")
        }
        val coreName = settings.retroArchCoreFor(game.systemId)
        if (coreName.isBlank()) {
            return LaunchResult.NotConfigured("Native core is not configured for ${game.systemId}.")
        }

        val prepared = withContext(Dispatchers.IO) {
            val coreFile = resolveCoreFile(context, settings, coreName)
                ?: return@withContext LaunchPreparation.MissingCore
            val romFile = resolveRomFile(context, game)
                ?: return@withContext LaunchPreparation.MissingRom
            if (isUnsupportedPlayStationImage(game, romFile)) {
                return@withContext LaunchPreparation.UnsupportedRom(
                    "This PlayStation file is ECM-compressed even though it is named .bin. Convert it back to a normal BIN/CUE image, then scan or load it again."
                )
            }
            val bootFile = prepareBootFile(context, game, romFile)

            val systemDir = File(context.filesDir, "native_system").apply { mkdirs() }
            if (settings.biosFolderUri.isNotBlank()) {
                copyFolderToPrivateStorage(context, settings.biosFolderUri, systemDir)
            } else if (game.systemId == "playstation") {
                copyCommonPlayStationBios(systemDir)
            }
            LaunchPreparation.Ready(coreFile, bootFile, systemDir)
        }
        when (prepared) {
            LaunchPreparation.MissingCore -> return LaunchResult.NotConfigured(
                "Core '$coreName' was not found. In Settings, pick your RetroArch folder or cores folder after installing/downloading that core."
            )
            LaunchPreparation.MissingRom -> return LaunchResult.Failed("Rainey could not read this ROM. Re-pick the ROM folder or use Load Content again.")
            is LaunchPreparation.UnsupportedRom -> return LaunchResult.Failed(prepared.message)
            is LaunchPreparation.Ready -> Unit
        }

        val intent = Intent(context, NativeEmulatorActivity::class.java).apply {
            putExtra(NativeEmulatorActivity.EXTRA_CORE_PATH, prepared.coreFile.absolutePath)
            putExtra(NativeEmulatorActivity.EXTRA_ROM_PATH, prepared.romFile.absolutePath)
            putExtra(NativeEmulatorActivity.EXTRA_SYSTEM_DIR, prepared.systemDir.absolutePath)
            putExtra(NativeEmulatorActivity.EXTRA_TOUCH_CONTROLS_ENABLED, settings.touchControlsEnabled)
            putExtra(NativeEmulatorActivity.EXTRA_TOUCH_CONTROLS_OPACITY, settings.touchControlsOpacity)
            putExtra(NativeEmulatorActivity.EXTRA_CONTROLLER_LAYOUT, settings.controllerLayout)
            putExtra(NativeEmulatorActivity.EXTRA_VIDEO_SCALE, settings.videoScale)
            putExtra(NativeEmulatorActivity.EXTRA_BORDER_STYLE, settings.borderStyle)
            putExtra(NativeEmulatorActivity.EXTRA_SYSTEM_ID, game.systemId)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        return try {
            context.startActivity(intent)
            LaunchResult.Launched
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to start native emulator.")
        }
    }

    private fun resolveRomFile(context: Context, game: Game): File? {
        val fileName = safeFileName(game.fileName.ifBlank { "${game.title}.${game.extension}" })
        val output = File(context.filesDir, "native_roms/${game.systemId}/$fileName").apply {
            parentFile?.mkdirs()
        }

        directFileFromExternalStorageUri(game.contentUri)
            ?.takeIf { it.exists() && it.canRead() }
            ?.let { directFile ->
                if (game.systemId !in privateCopySystems) return directFile
                directFile.inputStream().use { input ->
                    output.outputStream().use { outputStream -> input.copyTo(outputStream) }
                }
                output.setReadable(true, true)
                return extractSingleRomFromZipIfNeeded(game, output) ?: output
            }

        return try {
            context.contentResolver.openInputStream(Uri.parse(game.contentUri))?.use { input ->
                output.outputStream().use { outputStream -> input.copyTo(outputStream) }
            } ?: return null
            output.setReadable(true, true)
            extractSingleRomFromZipIfNeeded(game, output) ?: output
        } catch (_: Exception) {
            null
        }
    }

    private fun prepareBootFile(context: Context, game: Game, romFile: File): File {
        if (game.systemId != "playstation" || !romFile.extension.equals("bin", ignoreCase = true)) {
            return romFile
        }

        val cueFile = File(context.filesDir, "native_cues/playstation/${romFile.nameWithoutExtension}.cue").apply {
            parentFile?.mkdirs()
        }
        val cueText = """
            FILE "${romFile.absolutePath}" BINARY
              TRACK 01 MODE2/2352
                INDEX 01 00:00:00
        """.trimIndent() + "\n"
        cueFile.writeText(cueText)
        cueFile.setReadable(true, true)
        return cueFile
    }

    private fun isUnsupportedPlayStationImage(game: Game, romFile: File): Boolean {
        if (game.systemId != "playstation") return false
        if (!romFile.extension.equals("bin", ignoreCase = true)) return false
        return try {
            romFile.inputStream().use { input ->
                val header = ByteArray(4)
                input.read(header) == header.size &&
                    header[0] == 'E'.code.toByte() &&
                    header[1] == 'C'.code.toByte() &&
                    header[2] == 'M'.code.toByte() &&
                    header[3] == 0.toByte()
            }
        } catch (_: Exception) {
            false
        }
    }

    private fun directFileFromExternalStorageUri(contentUri: String): File? {
        val uri = Uri.parse(contentUri)
        if (uri.authority != "com.android.externalstorage.documents") return null
        val lastSegment = uri.lastPathSegment ?: return null
        val documentId = if (lastSegment.contains("document/")) {
            lastSegment.substringAfter("document/")
        } else {
            lastSegment
        }
        val separator = documentId.indexOf(':')
        if (separator <= 0) return null
        val volume = documentId.substring(0, separator)
        val relativePath = Uri.decode(documentId.substring(separator + 1))
        if (relativePath.isBlank()) return null
        return when (volume) {
            "primary" -> File("/storage/emulated/0", relativePath)
            else -> File("/storage/$volume", relativePath)
        }
    }

    private fun extractSingleRomFromZipIfNeeded(game: Game, zipFile: File): File? {
        if (game.extension.lowercase() != "zip" || game.systemId == "arcade") return null
        val allowedExtensions = cartridgeZipExtensions[game.systemId].orEmpty()
        if (allowedExtensions.isEmpty()) return null

        ZipInputStream(zipFile.inputStream()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory) continue
                val entryName = entry.name.substringAfterLast('/')
                val extension = entryName.substringAfterLast('.', missingDelimiterValue = "").lowercase()
                if (extension in allowedExtensions) {
                    val extracted = File(zipFile.parentFile, safeFileName(entryName))
                    extracted.outputStream().use { output -> zip.copyTo(output) }
                    extracted.setReadable(true, true)
                    return extracted
                }
            }
        }
        return null
    }

    private fun resolveCoreFile(context: Context, settings: UserSettings, coreName: String): File? {
        bundledCoreFile(context, coreName)?.let { return it }
        bundledCoreFromApk(context, coreName)?.let { return it }
        if (settings.retroArchCoresFolderUri.isBlank()) return null
        return copyCoreToPrivateStorage(context, settings.retroArchCoresFolderUri, coreName)
    }

    private fun bundledCoreFile(context: Context, coreName: String): File? {
        val nativeDir = File(context.applicationInfo.nativeLibraryDir)
        return coreNameCandidates(coreName)
            .map { File(nativeDir, it) }
            .firstOrNull { it.exists() && it.canRead() }
    }

    private fun bundledCoreFromApk(context: Context, coreName: String): File? {
        val fileName = coreName.substringAfterLast('/')
        val candidates = coreNameCandidates(coreName)
        val output = File(context.filesDir, "bundled_cores/$fileName").apply {
            parentFile?.mkdirs()
        }

        return try {
            ZipFile(context.applicationInfo.sourceDir).use { apk ->
                for (abi in Build.SUPPORTED_ABIS) {
                    for (candidate in candidates) {
                        val entry = apk.getEntry("lib/$abi/$candidate") ?: continue
                        apk.getInputStream(entry).use { input ->
                            output.outputStream().use { target -> input.copyTo(target) }
                        }
                        output.setReadable(true, true)
                        output.setExecutable(true, true)
                        return output
                    }
                }
            }
            null
        } catch (_: Exception) {
            null
        }
    }

    private fun coreNameCandidates(coreName: String): List<String> {
        val fileName = coreName.substringAfterLast('/')
        val cleanName = fileName.removePrefix("lib")
        return listOf(
            coreName,
            fileName,
            cleanName,
            "lib$coreName",
            "lib$fileName"
        ).distinct()
    }

    private fun copyCoreToPrivateStorage(context: Context, coresFolderUri: String, coreName: String): File? {
        if (coreName.startsWith("/") || coreName.startsWith("file:")) {
            return File(Uri.parse(coreName).path ?: coreName).takeIf { it.exists() }
        }

        val root = DocumentFile.fromTreeUri(context, Uri.parse(coresFolderUri)) ?: return null
        val coreDocument = findCoreDocument(root, coreName) ?: return null
        val output = File(context.filesDir, "native_cores/$coreName").apply { parentFile?.mkdirs() }
        context.contentResolver.openInputStream(coreDocument.uri)?.use { input ->
            output.outputStream().use { outputStream -> input.copyTo(outputStream) }
        } ?: return null
        output.setReadable(true, true)
        output.setExecutable(true, true)
        return output
    }

    private fun copyFolderToPrivateStorage(context: Context, folderUri: String, destination: File) {
        val root = DocumentFile.fromTreeUri(context, Uri.parse(folderUri)) ?: return
        copyChildren(context, root, destination)
    }

    private fun copyCommonPlayStationBios(destination: File) {
        commonPlayStationBiosFolders
            .map(::File)
            .filter { it.isDirectory && it.canRead() }
            .forEach { folder ->
                folder.listFiles()
                    .orEmpty()
                    .filter { file -> file.isFile && file.canRead() && file.name.isPlayStationBiosName() }
                    .forEach { source ->
                        runCatching {
                            val target = File(destination, source.name.lowercase())
                            if (!target.exists() || target.length() != source.length()) {
                                source.inputStream().use { input ->
                                    target.outputStream().use { output -> input.copyTo(output) }
                                }
                                target.setReadable(true, true)
                            }
                        }
                    }
            }
    }

    private fun copyChildren(context: Context, source: DocumentFile, destination: File) {
        if (!source.isDirectory) return
        source.listFiles().forEach { child ->
            if (child.isDirectory) {
                copyChildren(context, child, File(destination, child.name ?: return@forEach).apply { mkdirs() })
            } else if (child.isFile) {
                val target = File(destination, child.name ?: return@forEach)
                context.contentResolver.openInputStream(child.uri)?.use { input ->
                    target.outputStream().use { output -> input.copyTo(output) }
                }
            }
        }
    }

    private fun findCoreDocument(root: DocumentFile, coreName: String): DocumentFile? {
        return findFileByName(root, coreName)
            ?: findFileByName(root, coreName.substringAfterLast('/'))
    }

    private fun findFileByName(root: DocumentFile, fileName: String): DocumentFile? {
        if (!root.isDirectory) return null
        root.listFiles().forEach { child ->
            if (child.isFile && child.name == fileName) return child
            if (child.isDirectory) {
                val nested = findFileByName(child, fileName)
                if (nested != null) return nested
            }
        }
        return null
    }

    private fun safeFileName(fileName: String): String {
        return fileName
            .replace(Regex("[\\\\/:*?\"<>|]"), "_")
            .take(180)
            .ifBlank { "content.rom" }
    }

    companion object {
        private val cartridgeZipExtensions = mapOf(
            "nes" to setOf("nes"),
            "snes" to setOf("sfc", "smc"),
            "genesis" to setOf("gen", "md", "bin"),
            "gb" to setOf("gb", "gbc"),
            "gba" to setOf("gba"),
            "n64" to setOf("z64", "n64", "v64"),
            "playstation" to setOf("bin", "img")
        )
        private val privateCopySystems = setOf("nes", "snes", "genesis", "gb", "gba", "n64")
        private val commonPlayStationBiosFolders = listOf(
            "/storage/1513-1451/RetroArch/.retroarch/system",
            "/storage/1513-1451/Emu/PS/bios",
            "/storage/emulated/0/RetroArch/.retroarch/system",
            "/storage/emulated/0/Emu/PS/bios"
        )
    }

    private sealed interface LaunchPreparation {
        data object MissingCore : LaunchPreparation
        data object MissingRom : LaunchPreparation
        data class UnsupportedRom(val message: String) : LaunchPreparation
        data class Ready(val coreFile: File, val romFile: File, val systemDir: File) : LaunchPreparation
    }
}

private fun String.isPlayStationBiosName(): Boolean {
    val normalized = lowercase()
    return (normalized.startsWith("scph") || normalized == "psxonpsp660.bin") &&
        (normalized.endsWith(".bin") || normalized.endsWith(".bios"))
}
