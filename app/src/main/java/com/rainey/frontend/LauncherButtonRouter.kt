package com.rainey.frontend

import android.app.ActivityManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.SystemClock
import android.util.Log

object LauncherButtonRouter {
    private const val TAG = "RaineyLauncherKey"
    private const val RECENT_PAUSE_WINDOW_MS = 1_500L
    private const val DUPLICATE_KEY_WINDOW_MS = 700L

    private var mainActivityVisible = false
    private var lastMainActivityPauseMs = 0L
    private var lastQuickButtonHandledMs = 0L

    fun markRaineyResumed() {
        mainActivityVisible = true
    }

    fun markRaineyPaused() {
        mainActivityVisible = false
        lastMainActivityPauseMs = SystemClock.elapsedRealtime()
    }

    fun isRaineyVisible(): Boolean = mainActivityVisible

    fun toggle(context: Context, source: String = "unknown") {
        val now = SystemClock.elapsedRealtime()
        if (now - lastQuickButtonHandledMs in 0..DUPLICATE_KEY_WINDOW_MS) {
            Log.i(TAG, "Ignoring duplicate launcher button event from $source")
            return
        }
        lastQuickButtonHandledMs = now

        if (isRaineyActiveForToggle()) {
            Log.i(TAG, "Rainey active; closing Rainey and routing launcher button to Android home from $source")
            closeRaineyAndOpenAndroidHome(context)
        } else {
            Log.i(TAG, "Rainey inactive; routing launcher button to Rainey from $source")
            openRainey(context)
        }
    }

    fun handleHomePressed(context: Context) {
        Log.i(TAG, "Android HOME received; closing Rainey and staying on Android home")
        closeRaineyAndOpenAndroidHome(context)
    }

    fun openRainey(context: Context) {
        context.startActivity(
            Intent(context, MainActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)
            }
        )
    }

    fun openAndroidHome(context: Context) {
        val launcherCandidates = listOf(
            ComponentName(
                "com.android.launcher3",
                "com.android.searchlauncher.SearchLauncher"
            ),
            ComponentName(
                "com.android.launcher3",
                "com.android.launcher3.uioverrides.QuickstepLauncher"
            )
        )

        launcherCandidates.firstOrNull { component ->
            runCatching {
                context.startActivity(Intent(Intent.ACTION_MAIN).apply {
                    this.component = component
                    addCategory(Intent.CATEGORY_HOME)
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                })
            }.onFailure {
                Log.w(TAG, "Unable to launch Android home candidate $component", it)
            }.isSuccess
        } ?: runCatching {
            context.startActivity(Intent(Intent.ACTION_MAIN).apply {
                addCategory(Intent.CATEGORY_HOME)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            })
        }.onFailure {
            Log.w(TAG, "Unable to launch Android home", it)
        }
    }

    fun closeRaineyAndOpenAndroidHome(context: Context) {
        mainActivityVisible = false
        lastMainActivityPauseMs = 0L
        openAndroidHome(context)
        closeMainActivityTask(context)
    }

    private fun closeMainActivityTask(context: Context) {
        val activityManager = context.getSystemService(ActivityManager::class.java) ?: return
        runCatching {
            activityManager.appTasks.forEach { task ->
                val baseActivity = task.taskInfo.baseActivity
                if (
                    baseActivity?.packageName == context.packageName &&
                    baseActivity.className == MainActivity::class.java.name
                ) {
                    task.finishAndRemoveTask()
                }
            }
        }.onFailure {
            Log.w(TAG, "Unable to remove Rainey main task", it)
        }
    }

    private fun isRaineyActiveForToggle(): Boolean {
        if (mainActivityVisible) return true
        val elapsedSincePause = SystemClock.elapsedRealtime() - lastMainActivityPauseMs
        return lastMainActivityPauseMs > 0L && elapsedSincePause in 0..RECENT_PAUSE_WINDOW_MS
    }
}
