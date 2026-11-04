package com.rainey.frontend.scanner

import android.content.Context
import android.content.Intent
import com.rainey.frontend.data.entity.Game
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class AndroidGameScanner(private val context: Context) {
    suspend fun scanLaunchableApps(): List<Game> = withContext(Dispatchers.IO) {
        val packageManager = context.packageManager
        val launcherIntent = Intent(Intent.ACTION_MAIN).apply {
            addCategory(Intent.CATEGORY_LAUNCHER)
        }

        packageManager.queryIntentActivities(launcherIntent, 0)
            .asSequence()
            .filter { it.activityInfo?.packageName != context.packageName }
            .distinctBy { it.activityInfo.packageName }
            .sortedBy { it.loadLabel(packageManager).toString().lowercase() }
            .map { app ->
                val packageName = app.activityInfo.packageName
                val title = app.loadLabel(packageManager).toString()
                Game(
                    systemId = "android",
                    title = title,
                    fileName = packageName,
                    contentUri = "package:$packageName",
                    extension = "app",
                    developer = packageName,
                    description = "Installed Android app. Launched directly from its app package."
                )
            }
            .toList()
    }

    suspend fun scanInstalledGames(): List<Game> = scanLaunchableApps()
}
