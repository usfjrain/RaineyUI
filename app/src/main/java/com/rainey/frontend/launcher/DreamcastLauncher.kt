package com.rainey.frontend.launcher

import android.app.ActivityManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.dreamcast.DreamcastOverlayService
import java.io.File
import kotlinx.coroutines.delay

class DreamcastLauncher {
    suspend fun launch(context: Context, game: Game): LaunchResult {
        if (game.systemId != "dreamcast") {
            return LaunchResult.NotConfigured("Dreamcast launcher only handles Dreamcast games.")
        }

        val target = launcherTargets.firstOrNull { context.isPackageInstalled(it.packageName) }
            ?: return LaunchResult.MissingEmulator(launcherTargets.first().packageName)
        val launchPath = resolveLaunchPath(game)
            ?: return LaunchResult.Failed("Dreamcast emulators on this device need direct file access. Put the ROM under shared storage, then rescan the Dreamcast folder.")
        if (!File(launchPath).exists()) {
            return LaunchResult.Failed("Dreamcast ROM not found at $launchPath. Rescan the Dreamcast folder or move the file back into shared storage.")
        }
        validateDreamcastImage(File(launchPath))?.let { return it }
        clearStaleDreamcastSession(context, target.packageName)

        val launchIntent = Intent(Intent.ACTION_VIEW).apply {
            setClassName(target.packageName, target.activityName)
            setDataAndType(fileLaunchUri(target, launchPath), "application/octet-stream")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        }

        return try {
            allowFileUriLaunch()
            context.startActivity(launchIntent)
            delay(OVERLAY_ATTACH_DELAY_MS)
            if (!DreamcastOverlayService.startIfAllowed(context, launchPath, target.packageName, target.activityName)) {
                DreamcastOverlayService.requestPermission(context)
            }
            LaunchResult.Launched
        } catch (_: ActivityNotFoundException) {
            LaunchResult.MissingEmulator(target.packageName)
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to launch Dreamcast emulator.")
        }
    }

    private suspend fun clearStaleDreamcastSession(context: Context, activePackageName: String) {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        launcherTargets
            .map { it.packageName }
            .distinct()
            .forEach { packageName ->
                runCatching {
                    activityManager?.killBackgroundProcesses(packageName)
                    Log.i(TAG, "Requested stale Dreamcast process cleanup for $packageName before launch.")
                }.onFailure { error ->
                    Log.w(TAG, "Could not request Dreamcast cleanup for $packageName: ${error.message}")
                }
            }
        delay(if (activePackageName == "com.flycast.emulator") FLYCAST_RESTART_DELAY_MS else REDREAM_RESTART_DELAY_MS)
    }

    private fun resolveLaunchPath(game: Game): String? {
        val directPath = directFileFromExternalStorageUri(game.contentUri) ?: return null
        val directFile = File(directPath)
        if (game.extension.equals("gdi", ignoreCase = true)) {
            findPreferredCompleteImage(directFile)?.let { return it.absolutePath }
        }
        if (game.extension.equals("bin", ignoreCase = true)) {
            findCueForBin(directFile)?.let { return it.absolutePath }
        }
        return directFile.absolutePath
    }

    private fun findPreferredCompleteImage(gdiFile: File): File? {
        val parent = gdiFile.parentFile ?: return null
        val baseName = gdiFile.nameWithoutExtension
        val alternatives = listOf(
            File(parent, "$baseName.chd"),
            File(parent, "$baseName.cdi")
        )
        return alternatives.firstOrNull { it.exists() && it.length() > 0L }
    }

    private fun validateDreamcastImage(imageFile: File): LaunchResult.Failed? {
        if (!imageFile.extension.equals("gdi", ignoreCase = true)) return null

        val missingTracks = runCatching {
            imageFile.readLines()
                .drop(1)
                .mapNotNull { line -> gdiTrackRegex.find(line)?.groupValues?.getOrNull(1) }
                .filter { trackName -> !File(imageFile.parentFile, trackName).exists() }
        }.getOrElse {
            return LaunchResult.Failed("Rainey could not read ${imageFile.name}. Recopy the Dreamcast game or use a .chd version.")
        }

        if (missingTracks.isEmpty()) return null
        val preview = missingTracks.take(3).joinToString()
        val suffix = if (missingTracks.size > 3) " and ${missingTracks.size - 3} more" else ""
        return LaunchResult.Failed(
            "${imageFile.name} is incomplete. It is missing $preview$suffix. Copy the full GDI track set into the same folder, or replace it with a complete .chd/.cdi."
        )
    }

    private fun findCueForBin(binFile: File): File? {
        val parent = binFile.parentFile ?: return null
        val exactCue = File(parent, "${binFile.nameWithoutExtension}.cue")
        if (exactCue.exists()) return exactCue

        val cueFiles = parent.listFiles { file ->
            file.isFile &&
                file.extension.equals("cue", ignoreCase = true) &&
                !file.name.startsWith("._")
        }.orEmpty()
        return cueFiles.singleOrNull()
    }

    private fun directFileFromExternalStorageUri(contentUri: String): String? {
        val uri = Uri.parse(contentUri)
        if (uri.authority != "com.android.externalstorage.documents") return null
        val documentId = uri.lastPathSegment
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

    private fun allowFileUriLaunch() {
        try {
            Class.forName("android.os.StrictMode")
                .getMethod("disableDeathOnFileUriExposure")
                .invoke(null)
        } catch (_: Exception) {
            // Older Dreamcast emulators require file:// handoff. If the platform
            // blocks it, startActivity will return a user-visible launch error.
        }
    }

    private fun fileLaunchUri(target: LauncherTarget, path: String): Uri {
        return if (target.packageName == FLYCAST_PACKAGE) {
            // This preloaded Flycast build passes Intent data straight to native
            // file IO. A file:// URI is treated as a literal path and fails stat().
            Uri.parse(path)
        } else {
            Uri.fromFile(File(path))
        }
    }

    private data class LauncherTarget(
        val packageName: String,
        val activityName: String
    )

    companion object {
        private const val TAG = "DreamcastLaunch"
        private const val FLYCAST_PACKAGE = "com.flycast.emulator"
        private const val REDREAM_PACKAGE = "io.recompiled.redream"
        private const val FLYCAST_RESTART_DELAY_MS = 850L
        private const val REDREAM_RESTART_DELAY_MS = 650L
        private const val OVERLAY_ATTACH_DELAY_MS = 350L
        private val gdiTrackRegex = Regex("\"([^\"]+)\"")
        private val launcherTargets = listOf(
            LauncherTarget(FLYCAST_PACKAGE, "com.flycast.emulator.MainActivity"),
            LauncherTarget(REDREAM_PACKAGE, "io.recompiled.redream.MainActivity")
        )
    }
}
