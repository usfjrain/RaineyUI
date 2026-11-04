package com.rainey.frontend.artwork

import android.content.Context
import android.content.ContentUris
import android.net.Uri
import android.provider.MediaStore
import com.rainey.frontend.data.entity.Game
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class ArtworkImportResult(
    val attempted: Int,
    val imported: Int
)

class ArtworkImporter(private val context: Context) {
    suspend fun importBoxArt(games: List<Game>): ArtworkImportResult = withContext(Dispatchers.IO) {
        var imported = 0
        games.forEach { game ->
            if (game.boxArtUri != null || game.systemId == "android") return@forEach
            val target = File(context.filesDir, "artwork/${game.systemId}/${game.id}.png")
            target.parentFile?.mkdirs()

            if (copyLocalMatch(game, target)) {
                imported += 1
                return@forEach
            }

            val source = sourceFor(game.systemId) ?: return@forEach
            if (downloadFirstMatch(source, game.title, target)) {
                imported += 1
            }
        }
        ArtworkImportResult(attempted = games.count { it.systemId != "android" && it.boxArtUri == null }, imported = imported)
    }

    private fun copyLocalMatch(game: Game, target: File): Boolean {
        val match = findBestLocalImage(game) ?: return false
        return runCatching {
            context.contentResolver.openInputStream(match)?.use { input ->
                target.outputStream().use { output -> input.copyTo(output) }
            }
            target.length() > 0L
        }.getOrDefault(false).also { success ->
            if (!success) target.delete()
        }
    }

    private fun findBestLocalImage(game: Game): Uri? {
        val titleKey = normalizeForMatch(game.title)
        if (titleKey.isBlank()) return null
        val titleWords = titleKey.split(' ').filter { it.length > 1 }.toSet()
        var best: LocalImageMatch? = null

        val projection = arrayOf(
            MediaStore.Images.Media._ID,
            MediaStore.Images.Media.DISPLAY_NAME,
            MediaStore.Images.Media.RELATIVE_PATH
        )

        context.contentResolver.query(
            MediaStore.Images.Media.EXTERNAL_CONTENT_URI,
            projection,
            null,
            null,
            "${MediaStore.Images.Media.DATE_MODIFIED} DESC"
        )?.use { cursor ->
            val idIndex = cursor.getColumnIndexOrThrow(MediaStore.Images.Media._ID)
            val nameIndex = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DISPLAY_NAME)
            val pathIndex = cursor.getColumnIndex(MediaStore.Images.Media.RELATIVE_PATH)

            while (cursor.moveToNext()) {
                val displayName = cursor.getString(nameIndex) ?: continue
                if (!isImageName(displayName)) continue

                val nameWithoutExtension = displayName.substringBeforeLast('.', missingDelimiterValue = displayName)
                val imageKey = normalizeForMatch(nameWithoutExtension)
                if (imageKey.isBlank()) continue

                val relativePath = if (pathIndex >= 0) cursor.getString(pathIndex).orEmpty() else ""
                val score = matchScore(titleKey, titleWords, imageKey, relativePath)
                if (score <= 0) continue

                val id = cursor.getLong(idIndex)
                val uri = ContentUris.withAppendedId(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, id)
                val current = LocalImageMatch(uri, score)
                if (best == null || current.score > best!!.score) best = current
                if (score >= EXACT_MATCH_SCORE) break
            }
        }

        return best?.uri
    }

    private fun matchScore(
        titleKey: String,
        titleWords: Set<String>,
        imageKey: String,
        relativePath: String
    ): Int {
        val pathBonus = when {
            relativePath.contains("download", ignoreCase = true) -> 4
            relativePath.contains("picture", ignoreCase = true) -> 3
            relativePath.contains("photo", ignoreCase = true) -> 2
            else -> 0
        }
        if (imageKey == titleKey) return EXACT_MATCH_SCORE + pathBonus
        if (imageKey.contains(titleKey) || titleKey.contains(imageKey)) return 70 + pathBonus

        val imageWords = imageKey.split(' ').filter { it.length > 1 }.toSet()
        val overlap = titleWords.intersect(imageWords).size
        if (overlap < 2) return 0
        val coverage = overlap.toFloat() / titleWords.size.coerceAtLeast(1)
        return if (coverage >= 0.55f) (40 + overlap * 4 + pathBonus) else 0
    }

    private fun normalizeForMatch(value: String): String {
        return value
            .lowercase(Locale.US)
            .replace(Regex("\\[[^]]*]"), " ")
            .replace(Regex("\\([^)]*\\)"), " ")
            .replace(Regex("\\b(usa|europe|japan|world|rev|disc|disk|track|cover|box|boxart|front)\\b"), " ")
            .replace(Regex("[^a-z0-9]+"), " ")
            .replace(Regex("\\s+"), " ")
            .trim()
    }

    private fun isImageName(name: String): Boolean {
        return name.substringAfterLast('.', missingDelimiterValue = "")
            .lowercase(Locale.US) in imageExtensions
    }

    private fun downloadFirstMatch(source: String, title: String, target: File): Boolean {
        candidatesFor(title).forEach { candidate ->
            val url = "https://thumbnails.libretro.com/${Uri.encode(source)}/Named_Boxarts/${Uri.encode(candidate)}"
            if (download(url, target)) return true
        }
        return false
    }

    private fun download(url: String, target: File): Boolean {
        val connection = (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 6_000
            readTimeout = 8_000
            requestMethod = "GET"
            instanceFollowRedirects = true
            setRequestProperty("User-Agent", "Rainey/0.1")
        }
        return runCatching {
            connection.inputStream.use { input ->
                if (connection.responseCode !in 200..299) return false
                target.outputStream().use { output -> input.copyTo(output) }
            }
            target.length() > 0L
        }.getOrDefault(false).also {
            connection.disconnect()
            if (!it) target.delete()
        }
    }

    private fun candidatesFor(title: String): List<String> {
        val clean = title.trim()
        val safe = clean.replace(Regex("[&*/:`<>?\\\\|]"), "_")
        val withoutDisc = clean.replace(Regex("\\s*\\(Disc\\s+\\d+\\).*", RegexOption.IGNORE_CASE), "")
        return listOf(clean, safe, withoutDisc, safe.replace("  ", " "))
            .filter { it.isNotBlank() }
            .distinct()
            .map { "$it.png" }
    }

    private fun sourceFor(systemId: String): String? {
        return when (systemId) {
            "nes" -> "Nintendo - Nintendo Entertainment System"
            "snes" -> "Nintendo - Super Nintendo Entertainment System"
            "genesis" -> "Sega - Mega Drive - Genesis"
            "playstation" -> "Sony - PlayStation"
            "psp" -> "Sony - PlayStation Portable"
            "dreamcast" -> "Sega - Dreamcast"
            "arcade" -> "MAME"
            "gb" -> "Nintendo - Game Boy"
            "gba" -> "Nintendo - Game Boy Advance"
            "n64" -> "Nintendo - Nintendo 64"
            "ds" -> "Nintendo - Nintendo DS"
            else -> null
        }
    }

    private data class LocalImageMatch(val uri: Uri, val score: Int)

    private companion object {
        const val EXACT_MATCH_SCORE = 100
        val imageExtensions = setOf("png", "jpg", "jpeg", "webp")
    }
}
