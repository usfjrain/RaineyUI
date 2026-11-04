package com.rainey.frontend.launcher

import android.app.ActivityManager
import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import com.rainey.frontend.data.entity.Game
import com.rainey.frontend.duckstation.DuckStationOverlayService
import java.io.File
import kotlinx.coroutines.delay

class DuckStationLauncher(
    private val packageName: String = DEFAULT_PACKAGE
) {
    suspend fun launch(context: Context, game: Game): LaunchResult {
        if (game.systemId != "playstation") {
            return LaunchResult.NotConfigured("DuckStation is only used for PlayStation games.")
        }
        if (!context.isPackageInstalled(packageName)) {
            return LaunchResult.MissingEmulator(packageName)
        }

        val bootPath = resolveBootPath(game)
        validateBootPath(bootPath)?.let { return it }
        val romUri = Uri.parse(game.contentUri)
        if (!DuckStationOverlayService.startIfAllowed(context, bootPath)) {
            DuckStationOverlayService.requestPermission(context)
            return LaunchResult.Failed("Enable Display over other apps for Rainey, then launch the PlayStation game again.")
        }
        clearStaleDuckStationSession(context)
        context.grantUriPermission(packageName, romUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

        val launchIntent = Intent().apply {
            setClassName(packageName, "$packageName.EmulationActivity")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)

            if (bootPath.startsWith("/")) {
                action = Intent.ACTION_MAIN
                Log.i(TAG, "Launching DuckStation with bootPath=$bootPath")
            } else {
                action = Intent.ACTION_VIEW
                setDataAndType(romUri, "application/octet-stream")
                clipData = ClipData.newUri(context.contentResolver, game.fileName, romUri)
                Log.i(TAG, "Launching DuckStation with contentUri=$romUri")
            }

            putExtra("bootPath", bootPath)
            putExtra("EmulationActivity_bootPath", bootPath)
        }

        return try {
            context.startActivity(launchIntent)
            LaunchResult.Launched
        } catch (_: ActivityNotFoundException) {
            LaunchResult.MissingEmulator(packageName)
        } catch (error: SecurityException) {
            LaunchResult.Failed(
                "DuckStation does not have storage permission for this PlayStation game. Open Android Settings > Apps > DuckStation > Permissions and allow file/storage access, then try again."
            )
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to launch DuckStation.")
        }
    }

    private suspend fun clearStaleDuckStationSession(context: Context) {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        runCatching {
            activityManager?.killBackgroundProcesses(packageName)
            Log.i(TAG, "Requested stale DuckStation process cleanup before launch.")
        }.onFailure { error ->
            Log.w(TAG, "Could not request DuckStation cleanup: ${error.message}")
        }
        delay(DUCKSTATION_RESTART_DELAY_MS)
    }

    private fun resolveBootPath(game: Game): String {
        val directPath = directFileFromExternalStorageUri(game.contentUri) ?: return game.contentUri
        val directFile = File(directPath)
        if (game.extension.equals("bin", ignoreCase = true)) {
            findCueForBin(directFile)?.let { return it.absolutePath }
        }
        return directFile.absolutePath
    }

    private fun validateBootPath(bootPath: String): LaunchResult.Failed? {
        if (!bootPath.startsWith("/") || !bootPath.endsWith(".cue", ignoreCase = true)) return null
        val cueFile = File(bootPath)
        if (!cueFile.exists()) {
            return LaunchResult.Failed("The PlayStation cue file is missing: ${cueFile.name}")
        }

        val missingBins = runCatching {
            cueFile.readLines()
                .mapNotNull { line ->
                    cueFileRegex.find(line)?.groupValues?.getOrNull(1)
                }
                .filter { referencedFile ->
                    !File(cueFile.parentFile, referencedFile).exists()
                }
        }.getOrElse { emptyList() }

        if (missingBins.isNotEmpty()) {
            return LaunchResult.Failed(
                "This PlayStation game is missing required disc data: ${missingBins.joinToString()}. Keep the .cue and matching .bin files in the same folder."
            )
        }
        return null
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

    companion object {
        const val DEFAULT_PACKAGE = "com.github.stenzek.duckstation"
        private const val TAG = "RaineyDuckStation"
        private const val DUCKSTATION_RESTART_DELAY_MS = 550L
        private val cueFileRegex = Regex("^\\s*FILE\\s+\"([^\"]+)\"", RegexOption.IGNORE_CASE)
    }
}
