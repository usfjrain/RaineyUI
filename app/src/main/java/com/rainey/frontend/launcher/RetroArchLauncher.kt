package com.rainey.frontend.launcher

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.settings.UserSettings
import java.io.File

class RetroArchLauncher {
    suspend fun launch(context: Context, game: Game, settings: UserSettings): LaunchResult {
        if (!settings.retroArchEnabled) {
            return LaunchResult.NotConfigured("RetroArch launching is disabled.")
        }

        val packageName = resolveRetroArchPackage(context, settings.retroArchPackageName.trim())
        if (packageName.isBlank()) {
            return LaunchResult.NotConfigured("RetroArch package name is not configured.")
        }
        val coreNameOrPath = settings.retroArchCoreFor(game.systemId).trim()
        if (coreNameOrPath.isBlank()) {
            return LaunchResult.NotConfigured("RetroArch core is not configured for ${game.systemId}.")
        }
        val resolvedCore = resolveCoreLocation(context, packageName, game.systemId, settings, coreNameOrPath)
            ?: return LaunchResult.NotConfigured(
                "RetroArch core '$coreNameOrPath' was not found. Pick a cores folder in Settings or enter a full core path."
            )

        val romUri = Uri.parse(game.contentUri)
        val directRomPath = directFileFromExternalStorageUri(game.contentUri)
        val contentForRetroArch = resolveContentForRetroArch(game, directRomPath) ?: directRomPath ?: romUri.toString()
        if (isBlockedExternalCore(packageName, resolvedCore)) {
            return LaunchResult.NotConfigured(
                "Android is blocking RetroArch from loading this core from external storage. Open standalone RetroArch, install the PlayStation core inside RetroArch, or install DuckStation."
            )
        }
        Log.i(TAG, "Launching ${game.title} package=$packageName core=$resolvedCore content=$contentForRetroArch")
        val launchIntent = retroArchIntent(
            packageName = packageName,
            romUri = Uri.parse(game.contentUri),
            directRomPath = contentForRetroArch.takeIf { it.startsWith("/") },
            contentForRetroArch = contentForRetroArch,
            resolvedCore = resolvedCore,
            settings = settings,
            explicit = true
        )
        val fallbackIntent = retroArchIntent(
            packageName = packageName,
            romUri = Uri.parse(game.contentUri),
            directRomPath = contentForRetroArch.takeIf { it.startsWith("/") },
            contentForRetroArch = contentForRetroArch,
            resolvedCore = resolvedCore,
            settings = settings,
            explicit = false
        )

        return try {
            if (directRomPath != null) allowFileUriLaunch()
            context.startActivity(launchIntent)
            LaunchResult.Launched
        } catch (explicitError: ActivityNotFoundException) {
            Log.w(TAG, "Explicit RetroArch launch failed for package=$packageName; retrying package intent.", explicitError)
            try {
                if (directRomPath != null) allowFileUriLaunch()
                context.startActivity(fallbackIntent)
                LaunchResult.Launched
            } catch (_: ActivityNotFoundException) {
                LaunchResult.MissingEmulator(packageName)
            } catch (fallbackError: Exception) {
                LaunchResult.Failed(fallbackError.message ?: "Unable to launch RetroArch.")
            }
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to launch RetroArch.")
        }
    }

    private fun retroArchIntent(
        packageName: String,
        romUri: Uri,
        directRomPath: String?,
        contentForRetroArch: String,
        resolvedCore: String,
        settings: UserSettings,
        explicit: Boolean
    ): Intent {
        return Intent(Intent.ACTION_VIEW).apply {
            setPackage(packageName)
            if (explicit) {
                setClassName(packageName, retroActivityClassName(packageName))
            }
            val dataUri = directRomPath?.let { rawFileUri(it) } ?: romUri
            setDataAndType(dataUri, "application/octet-stream")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (dataUri.scheme == "content") addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)

            putExtra("ROM", contentForRetroArch)
            putExtra("LIBRETRO", resolvedCore)
            commonConfigPath()?.let { putExtra("CONFIGFILE", it) }
            putExtra("LOAD_CONTENT", contentForRetroArch)
            putExtra("CONTENT", contentForRetroArch)
            putExtra("EXTRA_CONTENT", contentForRetroArch)
            putExtra("EXTRA_CORE", extraCoreId(resolvedCore))
            if (settings.biosFolderUri.isNotBlank()) {
                putExtra("SYSTEM_DIRECTORY", settings.biosFolderUri)
                putExtra("BIOS", settings.biosFolderUri)
            }
        }
    }

    private fun resolveRetroArchPackage(context: Context, configuredPackage: String): String {
        if (configuredPackage == "com.retroarch" && context.isPackageInstalled("com.retroarch.aarch64")) {
            return "com.retroarch.aarch64"
        }
        if (configuredPackage.isNotBlank() && context.isPackageInstalled(configuredPackage)) {
            return configuredPackage
        }
        return listOf("com.retroarch.aarch64", "com.retroarch")
            .firstOrNull { context.isPackageInstalled(it) }
            ?: configuredPackage
    }

    private fun resolveCoreLocation(
        context: Context,
        packageName: String,
        systemId: String,
        settings: UserSettings,
        coreNameOrPath: String
    ): String? {
        installedSplitCorePath(context, packageName, systemId)?.let { return it }

        if (coreNameOrPath.startsWith("/") ||
            coreNameOrPath.startsWith("file:") ||
            coreNameOrPath.startsWith("content:")
        ) {
            return coreNameOrPath
        }

        commonCoreLocationPaths(coreNameOrPath).firstOrNull { File(it).exists() && File(it).canRead() }?.let {
            return it
        }

        val sdCardRetroArchCore = commonCoreLocationPaths(coreNameOrPath).firstOrNull {
            it.startsWith("/storage/1513-1451/RetroArch/")
        }
        if (sdCardRetroArchCore != null) {
            return sdCardRetroArchCore
        }

        val coresFolderUri = settings.retroArchCoresFolderUri
        if (coresFolderUri.isBlank()) return coreNameOrPath

        val root = DocumentFile.fromTreeUri(context, Uri.parse(coresFolderUri)) ?: return null
        val match = findFileByName(root, coreNameOrPath)
        return match?.uri?.toString()
    }

    private fun installedSplitCorePath(context: Context, packageName: String, systemId: String): String? {
        if (packageName != "com.retroarch") return null
        val splitPaths = runCatching {
            context.packageManager.getApplicationInfo(packageName, 0).splitSourceDirs.orEmpty()
        }.getOrDefault(emptyArray())
        return when (systemId) {
            "playstation" -> splitPaths
                .firstOrNull { it.contains("split_core_duckstation.config.arm64_v8a") }
                ?.let { "$it!/lib/arm64-v8a/libduckstation.so" }
            "snes" -> splitPaths
                .firstOrNull { it.contains("split_core_snes9x.config.arm64_v8a") }
                ?.let { "$it!/lib/arm64-v8a/libsnes9x.so" }
            "arcade" -> splitPaths
                .firstOrNull { it.contains("split_core_fbneo.config.arm64_v8a") }
                ?.let { "$it!/lib/arm64-v8a/libfbneo.so" }
            "n64" -> splitPaths
                .firstOrNull { it.contains("split_core_mupen64plus_next_gles2.config.arm64_v8a") }
                ?.let { "$it!/lib/arm64-v8a/libmupen64plus_next_gles2.so" }
            "ds" -> splitPaths
                .firstOrNull {
                    it.contains("split_core_melondsds.config.arm64_v8a") ||
                        it.contains("split_core_desmume.config.arm64_v8a")
                }
                ?.let { splitPath ->
                    val library = if (splitPath.contains("desmume")) "libdesmume.so" else "libmelondsds.so"
                    "$splitPath!/lib/arm64-v8a/$library"
                }
            else -> null
        }
    }

    private fun resolveContentForRetroArch(game: Game, directRomPath: String?): String? {
        if (game.systemId != "playstation" || !game.extension.equals("bin", ignoreCase = true)) return directRomPath
        val binFile = directRomPath?.let(::File) ?: return directRomPath
        val cueFile = findCueForBin(binFile)
        if (cueFile != null) return cueFile.absolutePath
        return directRomPath
    }

    private fun findCueForBin(binFile: File): File? {
        val parent = binFile.parentFile ?: return null
        val exactCue = File(parent, "${binFile.nameWithoutExtension}.cue")
        if (exactCue.exists()) return exactCue
        val cueFiles = parent.listFiles { file ->
            file.isFile && file.extension.equals("cue", ignoreCase = true) && !file.name.startsWith("._")
        }.orEmpty()
        return cueFiles.singleOrNull()
    }

    private fun isBlockedExternalCore(packageName: String, resolvedCore: String): Boolean {
        if (packageName == "com.retroarch") return false
        if (resolvedCore.contains(".apk!/lib/")) return false
        return resolvedCore.startsWith("/sdcard/") ||
            resolvedCore.startsWith("/storage/emulated/") ||
            resolvedCore.startsWith("/storage/1513-1451/")
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

    private fun commonCoreLocationPaths(coreName: String): List<String> {
        val fileName = coreName.substringAfterLast('/')
        return listOf(
            "/storage/1513-1451/RetroArch/.retroarch/cores/$fileName",
            "/storage/emulated/0/RetroArch/cores/$fileName",
            "/storage/emulated/0/RetroArch/.retroarch/cores/$fileName",
            "/sdcard/RetroArch/cores/$fileName",
            "/sdcard/RetroArch/.retroarch/cores/$fileName"
        )
    }

    private fun directFileFromExternalStorageUri(contentUri: String): String? {
        if (contentUri.startsWith("/")) return contentUri
        val uri = Uri.parse(contentUri)
        if (uri.scheme == "file") return uri.path
        if (uri.authority != "com.android.externalstorage.documents") return null
        val documentId = runCatching { DocumentsContract.getDocumentId(uri) }.getOrNull()
            ?: uri.lastPathSegment
            ?.substringAfter("document/", missingDelimiterValue = uri.lastPathSegment ?: "")
            ?: return null
        val separator = documentId.indexOf(':')
        if (separator <= 0) return null
        val volume = documentId.substring(0, separator)
        val relativePath = Uri.decode(documentId.substring(separator + 1))
        if (relativePath.isBlank()) return null
        return when (volume) {
            "primary" -> "/storage/emulated/0/$relativePath"
            else -> "/storage/$volume/$relativePath"
        }
    }

    private fun commonConfigPath(): String? {
        val configPaths = listOf(
            "/storage/1513-1451/RetroArch/.retroarch/retroarch.cfg",
            "/storage/emulated/0/RetroArch/.retroarch/retroarch.cfg",
            "/storage/emulated/0/RetroArch/retroarch.cfg",
            "/sdcard/RetroArch/.retroarch/retroarch.cfg",
            "/sdcard/RetroArch/retroarch.cfg"
        )
        return configPaths.firstOrNull { File(it).exists() }
            ?: configPaths.firstOrNull { it.startsWith("/storage/1513-1451/RetroArch/") }
    }

    private fun allowFileUriLaunch() {
        try {
            Class.forName("android.os.StrictMode")
                .getMethod("disableDeathOnFileUriExposure")
                .invoke(null)
        } catch (_: Exception) {
            // RetroArch 1.9.x on this device only honors direct file:// ROM handoff.
        }
    }

    private fun rawFileUri(path: String): Uri = Uri.fromFile(File(path))

    private fun retroActivityClassName(packageName: String): String {
        return when (packageName) {
            "com.retroarch.aarch64" -> "com.retroarch.browser.retroactivity.RetroActivityFuture"
            else -> "$packageName.browser.retroactivity.RetroActivityFuture"
        }
    }

    private fun extraCoreId(resolvedCore: String): String {
        return when {
            resolvedCore.contains("duckstation", ignoreCase = true) -> "core_duckstation"
            resolvedCore.contains("pcsx", ignoreCase = true) -> "core_pcsx_rearmed"
            resolvedCore.contains("snes9x", ignoreCase = true) -> "core_snes9x"
            resolvedCore.contains("fbneo", ignoreCase = true) -> "core_fbneo"
            resolvedCore.contains("mupen64plus", ignoreCase = true) -> "core_mupen64plus_next_gles2"
            else -> "core_${resolvedCore.substringAfterLast('/').substringBefore("_libretro").substringBefore(".so")}"
        }
    }

    private companion object {
        const val TAG = "RaineyRetroArch"
    }
}
