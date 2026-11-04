package com.rainey.frontend.dreamcast

import android.app.ActivityManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.provider.Settings
import android.net.Uri
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import com.rainey.frontend.MainActivity
import java.io.File

class DreamcastOverlayService : Service() {
    private var windowManager: WindowManager? = null
    private var overlayView: View? = null
    private var currentLaunchPath: String? = null
    private var currentEmulatorPackage: String? = null
    private var currentEmulatorActivity: String? = null
    private val handler = Handler(Looper.getMainLooper())

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!Settings.canDrawOverlays(this)) {
            stopSelf()
            return START_NOT_STICKY
        }
        intent?.getStringExtra(EXTRA_LAUNCH_PATH)?.takeIf { it.isNotBlank() }?.let { currentLaunchPath = it }
        intent?.getStringExtra(EXTRA_EMULATOR_PACKAGE)?.takeIf { it.isNotBlank() }?.let { currentEmulatorPackage = it }
        intent?.getStringExtra(EXTRA_EMULATOR_ACTIVITY)?.takeIf { it.isNotBlank() }?.let { currentEmulatorActivity = it }
        showCollapsed()
        return START_STICKY
    }

    override fun onDestroy() {
        removeOverlay()
        super.onDestroy()
    }

    private fun showCollapsed() {
        removeOverlay()
        val button = Button(this).apply {
            text = "DC"
            textSize = 14f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(120, 45, 255))
            setOnClickListener { showMenu() }
        }
        overlayView = button
        addOverlayView(
            button,
            params(width = dp(60), height = dp(56), gravity = Gravity.END or Gravity.CENTER_VERTICAL)
        )
    }

    private fun showMenu() {
        removeOverlay()
        val root = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(135, 0, 0, 0))
            isFocusable = true
        }
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(16), dp(18), dp(16))
            setBackgroundColor(Color.rgb(12, 11, 28))
        }
        panel.addView(TextView(this).apply {
            text = "Rainey"
            textSize = 24f
            setTextColor(Color.WHITE)
        })
        panel.addView(TextView(this).apply {
            text = "Dreamcast quick menu"
            textSize = 13f
            setTextColor(Color.rgb(126, 231, 255))
            setPadding(0, 0, 0, dp(10))
        })
        panel.addView(menuButton("Resume") { showCollapsed() })
        panel.addView(sectionLabel("Video resolution"))
        panel.addView(resolutionRow())
        panel.addView(sectionLabel("State slot"))
        panel.addView(slotRow())
        panel.addView(menuButton("Save", "Flycast handles instant save states inside its own menu. Rainey enabled auto-save for this slot.") {
            updateFlycastConfig("Dreamcast.AutoSaveState", "yes")
            Toast.makeText(this, "Auto-save enabled for Flycast slot ${readSlot()}. Use Flycast menu for instant save.", Toast.LENGTH_LONG).show()
        })
        panel.addView(menuButton("Load", "Flycast will auto-load this slot on next boot when available.") {
            updateFlycastConfig("Dreamcast.AutoLoadState", "yes")
            Toast.makeText(this, "Auto-load enabled for Flycast slot ${readSlot()}. Restart game to load.", Toast.LENGTH_LONG).show()
        })
        panel.addView(menuButton("Back to Rainey") {
            startActivity(Intent(this, MainActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
            })
            stopSelf()
        })
        panel.addView(menuButton("Hide Overlay") { stopSelf() })
        panel.addView(TextView(this).apply {
            text = "Resolution applies by restarting the current Dreamcast game. Save/load commands are limited by the external emulator."
            textSize = 11f
            setTextColor(Color.rgb(190, 196, 214))
            setPadding(0, dp(10), 0, 0)
        })
        root.addView(
            panel,
            FrameLayout.LayoutParams(dp(340), FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER)
        )
        root.setOnClickListener { showCollapsed() }
        panel.setOnClickListener { }
        overlayView = root
        addOverlayView(
            root,
            params(width = WindowManager.LayoutParams.MATCH_PARENT, height = WindowManager.LayoutParams.MATCH_PARENT)
        )
    }

    private fun resolutionRow(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            for (scale in 1..5) {
                addView(menuButton("${scale}x") {
                    applyResolution(scale)
                }, LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                    setMargins(dp(2), dp(2), dp(2), dp(2))
                })
            }
        }
    }

    private fun slotRow(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(menuButton("-") { changeSlot(-1) }, LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                setMargins(dp(2), dp(2), dp(2), dp(2))
            })
            addView(menuButton("Slot ${readSlot()}") {
                Toast.makeText(this@DreamcastOverlayService, "Current Flycast state slot: ${readSlot()}", Toast.LENGTH_SHORT).show()
            }, LinearLayout.LayoutParams(0, dp(48), 2f).apply {
                setMargins(dp(2), dp(2), dp(2), dp(2))
            })
            addView(menuButton("+") { changeSlot(1) }, LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                setMargins(dp(2), dp(2), dp(2), dp(2))
            })
        }
    }

    private fun sectionLabel(label: String): TextView {
        return TextView(this).apply {
            text = label
            textSize = 13f
            setTextColor(Color.rgb(126, 231, 255))
            setPadding(0, dp(8), 0, dp(4))
        }
    }

    private fun menuButton(label: String, onClick: () -> Unit): Button {
        return Button(this).apply {
            text = label
            textSize = 16f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(44, 38, 82))
            setOnClickListener { onClick() }
        }
    }

    private fun menuButton(label: String, hint: String, onClick: () -> Unit): Button {
        return menuButton(label) {
            onClick()
            Toast.makeText(this, hint, Toast.LENGTH_SHORT).show()
        }
    }

    private fun menuButton(label: String, onClick: () -> Unit, layoutParams: LinearLayout.LayoutParams): Button {
        return menuButton(label, onClick).apply {
            this.layoutParams = layoutParams
            textSize = 13f
        }
    }

    private fun applyResolution(scale: Int) {
        val resolution = 480 * scale.coerceIn(1, 5)
        val updated = updateFlycastConfig("rend.Resolution", resolution.toString())
        if (!updated) {
            Toast.makeText(this, "Could not update Flycast resolution config.", Toast.LENGTH_LONG).show()
            return
        }
        Toast.makeText(this, "Dreamcast resolution set to ${scale}x", Toast.LENGTH_SHORT).show()
        restartCurrentGame()
    }

    private fun changeSlot(delta: Int) {
        val next = (readSlot() + delta).coerceIn(0, 9)
        if (updateFlycastConfig("Dreamcast.SavestateSlot", next.toString())) {
            Toast.makeText(this, "Flycast state slot set to $next", Toast.LENGTH_SHORT).show()
            showMenu()
        }
    }

    private fun readSlot(): Int {
        val config = flycastConfigFile() ?: return 0
        return config.readLinesOrEmpty()
            .firstOrNull { it.trim().startsWith("Dreamcast.SavestateSlot") }
            ?.substringAfter('=')
            ?.trim()
            ?.toIntOrNull()
            ?.coerceIn(0, 9)
            ?: 0
    }

    private fun updateFlycastConfig(key: String, value: String): Boolean {
        val config = flycastConfigFile() ?: return false
        val lines = config.readLinesOrEmpty().toMutableList()
        val index = lines.indexOfFirst { it.trim().startsWith("$key ") || it.trim().startsWith("$key=") }
        if (index >= 0) {
            lines[index] = "$key = $value"
        } else {
            val configHeader = lines.indexOfFirst { it.trim().equals("[config]", ignoreCase = true) }
            if (configHeader >= 0) {
                lines.add(configHeader + 1, "$key = $value")
            } else {
                lines += "[config]"
                lines += "$key = $value"
            }
        }
        return runCatching {
            config.writeText(lines.joinToString(separator = "\n", postfix = "\n"))
        }.isSuccess
    }

    private fun flycastConfigFile(): File? {
        val launchPath = currentLaunchPath
        val candidates = buildList {
            if (!launchPath.isNullOrBlank()) {
                File(launchPath).parentFile?.let { parent ->
                    add(File(parent, "emu.cfg"))
                    parent.parentFile?.let { add(File(it, "emu.cfg")) }
                }
            }
            add(File("/storage/0000-0000/dreamcast/emu.cfg"))
            add(File("/storage/emulated/0/Android/data/com.flycast.emulator/files/emu.cfg"))
        }
        return candidates.firstOrNull { it.exists() && it.canRead() && it.canWrite() }
    }

    private fun restartCurrentGame() {
        val launchPath = currentLaunchPath
        val packageName = currentEmulatorPackage
        val activityName = currentEmulatorActivity
        if (launchPath.isNullOrBlank() || packageName.isNullOrBlank() || activityName.isNullOrBlank()) {
            showCollapsed()
            Toast.makeText(this, "Restart the game from Rainey to apply changes.", Toast.LENGTH_LONG).show()
            return
        }
        showCollapsed()
        startActivity(Intent(this, MainActivity::class.java).apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
        })
        handler.postDelayed({
            runCatching {
                (getSystemService(ACTIVITY_SERVICE) as? ActivityManager)?.killBackgroundProcesses(packageName)
            }
        }, 450L)
        handler.postDelayed({
            runCatching {
                startActivity(Intent(Intent.ACTION_VIEW).apply {
                    setClassName(packageName, activityName)
                    setDataAndType(fileLaunchUri(packageName, launchPath), "application/octet-stream")
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                })
            }.onFailure {
                Toast.makeText(this, "Restart the game from Rainey to apply changes.", Toast.LENGTH_LONG).show()
            }
        }, 950L)
    }

    private fun removeOverlay() {
        val view = overlayView ?: return
        runCatching { windowManager()?.removeView(view) }
        overlayView = null
    }

    private fun addOverlayView(view: View, layoutParams: WindowManager.LayoutParams) {
        runCatching {
            windowManager().addView(view, layoutParams)
        }.onFailure { error ->
            Log.w(TAG, "Could not add Dreamcast overlay: ${error.message}")
            overlayView = null
            stopSelf()
        }
    }

    private fun params(width: Int, height: Int, gravity: Int = Gravity.CENTER): WindowManager.LayoutParams {
        return WindowManager.LayoutParams(
            width,
            height,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            } else {
                @Suppress("DEPRECATION")
                WindowManager.LayoutParams.TYPE_PHONE
            },
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        ).apply {
            this.gravity = gravity
        }
    }

    private fun windowManager(): WindowManager {
        return windowManager ?: (getSystemService(WINDOW_SERVICE) as WindowManager).also {
            windowManager = it
        }
    }

    private fun File.readLinesOrEmpty(): List<String> = runCatching { readLines() }.getOrDefault(emptyList())

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    private fun fileLaunchUri(packageName: String, path: String): Uri {
        return if (packageName == FLYCAST_PACKAGE) {
            // This Flycast build passes Intent data straight to native file IO.
            // Supplying file:// makes it try to stat the literal URI string.
            Uri.parse(path)
        } else {
            Uri.fromFile(File(path))
        }
    }

    companion object {
        private const val TAG = "DreamcastOverlay"
        private const val FLYCAST_PACKAGE = "com.flycast.emulator"
        private const val EXTRA_LAUNCH_PATH = "rainey_dreamcast_launch_path"
        private const val EXTRA_EMULATOR_PACKAGE = "rainey_dreamcast_emulator_package"
        private const val EXTRA_EMULATOR_ACTIVITY = "rainey_dreamcast_emulator_activity"

        fun startIfAllowed(
            context: Context,
            launchPath: String,
            emulatorPackage: String,
            emulatorActivity: String
        ): Boolean {
            if (!Settings.canDrawOverlays(context)) return false
            return runCatching {
                context.startService(Intent(context, DreamcastOverlayService::class.java).apply {
                    putExtra(EXTRA_LAUNCH_PATH, launchPath)
                    putExtra(EXTRA_EMULATOR_PACKAGE, emulatorPackage)
                    putExtra(EXTRA_EMULATOR_ACTIVITY, emulatorActivity)
                })
                true
            }.getOrElse { error ->
                Log.w(TAG, "Could not start Dreamcast overlay: ${error.message}")
                false
            }
        }

        fun requestPermission(context: Context) {
            Toast.makeText(context, "Enable Display over other apps for Rainey Dreamcast quick menu.", Toast.LENGTH_LONG).show()
            context.startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION).apply {
                data = android.net.Uri.parse("package:${context.packageName}")
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            })
        }
    }
}
