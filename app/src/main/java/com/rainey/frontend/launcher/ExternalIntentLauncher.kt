package com.rainey.frontend.launcher

import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.StrictMode
import androidx.core.content.FileProvider
import com.rainey.frontend.data.entity.Game
import java.io.File

class ExternalIntentLauncher(
    private val packageMappings: Map<String, String?> = defaultPackageMappings
) : EmulatorLauncher {
    override suspend fun launch(context: Context, game: Game): LaunchResult {
        if (game.systemId == "android" && game.contentUri.startsWith("package:")) {
            val packageName = game.contentUri.removePrefix("package:")
            val launchIntent = context.packageManager.getLaunchIntentForPackage(packageName)
                ?: explicitAndroidLaunchIntent(packageName)
                ?: return LaunchResult.MissingEmulator(packageName)
            return try {
                launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                context.startActivity(launchIntent)
                LaunchResult.Launched
            } catch (error: Exception) {
                LaunchResult.Failed(error.message ?: "Unable to launch Android game.")
            }
        }

        if (game.systemId == "ds") {
            launchMelonDs(context, game)?.let { return it }
        }

        val packageName = packageMappings[game.systemId]
        if (packageName.isNullOrBlank()) {
            return LaunchResult.MissingEmulator(packageName)
        }
        if (!context.isPackageInstalled(packageName)) {
            return LaunchResult.MissingEmulator(packageName)
        }

        val romUri = Uri.parse(game.contentUri)
        val launchUri = directFileFromExternalStorageUri(game.contentUri)
            ?.takeIf { it.exists() && it.canRead() }
            ?.let { file ->
                disableFileUriExposureCrash()
                Uri.fromFile(file)
            }
            ?: romUri
        val launchIntent = Intent(Intent.ACTION_VIEW).apply {
            setPackage(packageName)
            setDataAndType(launchUri, mimeFor(game.extension))
            clipData = ClipData.newRawUri(game.fileName, launchUri)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }

        return try {
            context.startActivity(launchIntent)
            LaunchResult.Launched
        } catch (missing: ActivityNotFoundException) {
            LaunchResult.MissingEmulator(packageName)
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to launch emulator.")
        }
    }

    private fun launchMelonDs(context: Context, game: Game): LaunchResult? {
        val packageName = "me.magnum.melonds"
        if (!context.isPackageInstalled(packageName)) return null

        val romUri = directFileFromExternalStorageUri(game.contentUri)
            ?.takeIf { it.exists() && it.canRead() }
            ?.let { file ->
                FileProvider.getUriForFile(
                    context,
                    "${context.packageName}.fileprovider",
                    file
                )
            }
            ?: Uri.parse(game.contentUri)
        val launchIntent = Intent("$packageName.LAUNCH_ROM").apply {
            setClassName(packageName, "me.magnum.melonds.ui.emulator.EmulatorActivity")
            addCategory(Intent.CATEGORY_DEFAULT)
            putExtra("uri", romUri.toString())
            clipData = ClipData.newRawUri(game.fileName, romUri)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }

        return try {
            context.startActivity(launchIntent)
            LaunchResult.Launched
        } catch (missing: ActivityNotFoundException) {
            null
        } catch (error: Exception) {
            LaunchResult.Failed(error.message ?: "Unable to launch melonDS.")
        }
    }

    private fun explicitAndroidLaunchIntent(packageName: String): Intent? {
        return when (packageName) {
            "com.raineyretro.courtsidechaos" -> Intent().apply {
                setClassName(packageName, "com.godot.game.GodotAppLauncher")
            }
            else -> null
        }
    }

    private fun mimeFor(extension: String): String {
        return when (extension.lowercase()) {
            "zip", "7z" -> "application/zip"
            "iso", "cso", "chd", "cue", "gdi", "cdi" -> "application/octet-stream"
            else -> "application/octet-stream"
        }
    }

    private fun directFileFromExternalStorageUri(contentUri: String): File? {
        val uri = Uri.parse(contentUri)
        if (uri.authority != "com.android.externalstorage.documents") return null
        val documentId = uri.lastPathSegment
            ?.substringAfter("document/")
            ?: return null
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

    private fun disableFileUriExposureCrash() {
        runCatching {
            StrictMode::class.java
                .getMethod("disableDeathOnFileUriExposure")
                .invoke(null)
        }
    }

    companion object {
        val defaultPackageMappings = mapOf(
            "nes" to "com.explusalpha.NesEmu",
            "snes" to "com.explusalpha.Snes9xPlus",
            "genesis" to "com.explusalpha.MdEmu",
            "playstation" to "com.github.stenzek.duckstation",
            "psp" to "org.ppsspp.ppsspp",
            "dreamcast" to "com.flycast.emulator",
            "arcade" to "com.joyemu.fbaapp",
            "gb" to "com.retroarch.aarch64",
            "gba" to "com.retroarch.aarch64",
            "n64" to "org.mupen64plusae.v3.fzurita.pro",
            "ds" to "me.magnum.melonds"
        )
    }
}
