package com.rainey.frontend.duckstation

import android.app.Service
import android.app.ActivityManager
import android.content.Intent
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.preference.PreferenceManager
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import com.rainey.frontend.MainActivity

class DuckStationOverlayService : Service() {
    private var windowManager: WindowManager? = null
    private var overlayView: View? = null
    private var expanded = false
    private var currentBootPath: String? = null
    private var selectedResolutionScale = 1
    private val handler = Handler(Looper.getMainLooper())

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!Settings.canDrawOverlays(this)) {
            stopSelf()
            return START_NOT_STICKY
        }
        intent?.getStringExtra(EXTRA_BOOT_PATH)?.takeIf { it.isNotBlank() }?.let {
            currentBootPath = it
        }
        if (intent?.action == ACTION_SET_RESOLUTION) {
            applyResolutionScale(intent.getIntExtra(EXTRA_RESOLUTION_SCALE, selectedResolutionScale))
            return START_STICKY
        }
        showCollapsed()
        return START_STICKY
    }

    override fun onDestroy() {
        removeOverlay()
        super.onDestroy()
    }

    private fun showCollapsed() {
        expanded = false
        removeOverlay()
        val button = Button(this).apply {
            text = "R"
            textSize = 18f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(22, 132, 255))
            setOnClickListener { showMenu() }
        }
        overlayView = button
        windowManager().addView(
            button,
            params(width = dp(56), height = dp(56), gravity = Gravity.END or Gravity.CENTER_VERTICAL)
        )
    }

    private fun showMenu() {
        expanded = true
        removeOverlay()
        val root = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(130, 0, 0, 0))
            isFocusable = true
        }
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(16), dp(18), dp(16))
            setBackgroundColor(Color.rgb(10, 14, 30))
        }
        panel.addView(TextView(this).apply {
            text = "Rainey"
            textSize = 24f
            setTextColor(Color.WHITE)
        })
        panel.addView(TextView(this).apply {
            text = "DuckStation quick menu"
            textSize = 13f
            setTextColor(Color.rgb(126, 231, 255))
            setPadding(0, 0, 0, dp(10))
        })
        panel.addView(menuButton("Resume") { showCollapsed() })
        panel.addView(TextView(this).apply {
            text = "Video resolution"
            textSize = 13f
            setTextColor(Color.rgb(126, 231, 255))
            setPadding(0, dp(8), 0, dp(4))
        })
        panel.addView(resolutionRow())
        panel.addView(menuButton("Back to Rainey") {
            startActivity(Intent(this, MainActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
            })
            stopSelf()
        })
        panel.addView(menuButton("Hide Overlay") { stopSelf() })
        panel.addView(TextView(this).apply {
            text = "Save/load are handled by DuckStation's own in-game menu. Android does not expose those actions to Rainey as public commands."
            textSize = 11f
            setTextColor(Color.rgb(190, 196, 214))
            setPadding(0, dp(10), 0, 0)
        })
        root.addView(
            panel,
            FrameLayout.LayoutParams(dp(320), FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER)
        )
        root.setOnClickListener { showCollapsed() }
        panel.setOnClickListener { }
        overlayView = root
        windowManager().addView(
            root,
            params(width = WindowManager.LayoutParams.MATCH_PARENT, height = WindowManager.LayoutParams.MATCH_PARENT)
        )
    }

    private fun resolutionRow(): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 0, 0, dp(8))
            for (scale in 1..5) {
                addView(menuButton("${scale}x") {
                    applyResolutionScale(scale)
                }, LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                    setMargins(dp(2), dp(2), dp(2), dp(2))
                })
            }
        }
    }

    private fun menuButton(label: String, onClick: () -> Unit): Button {
        return Button(this).apply {
            text = label
            textSize = 16f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.rgb(38, 45, 76))
            setOnClickListener { onClick() }
        }
    }

    private fun menuButton(label: String, onClick: () -> Unit, layoutParams: LinearLayout.LayoutParams): Button {
        return menuButton(label, onClick).apply {
            this.layoutParams = layoutParams
            textSize = 13f
        }
    }

    private fun applyResolutionScale(scale: Int) {
        selectedResolutionScale = scale.coerceIn(1, 5)
        val saved = saveDuckStationResolutionScale(selectedResolutionScale)
        if (!saved) {
            Toast.makeText(
                this,
                "Could not write DuckStation resolution setting. Open DuckStation settings once, then try again.",
                Toast.LENGTH_LONG
            ).show()
            return
        }

        Toast.makeText(this, "DuckStation resolution set to ${selectedResolutionScale}x", Toast.LENGTH_SHORT).show()
        val bootPath = currentBootPath
        if (bootPath.isNullOrBlank()) {
            Toast.makeText(this, "Restart the game from Rainey to apply ${selectedResolutionScale}x.", Toast.LENGTH_LONG).show()
            showCollapsed()
            return
        }

        restartDuckStationGame(bootPath)
    }

    private fun saveDuckStationResolutionScale(scale: Int): Boolean {
        return runCatching {
            val duckContext = createPackageContext(DUCKSTATION_PACKAGE, Context.CONTEXT_IGNORE_SECURITY)
            val preferences = PreferenceManager.getDefaultSharedPreferences(duckContext)
            preferences.edit()
                .putInt("GPU/ResolutionScale", scale.coerceIn(1, 5))
                .commit()
        }.getOrDefault(false)
    }

    private fun restartDuckStationGame(bootPath: String) {
        showCollapsed()
        startActivity(Intent(this, MainActivity::class.java).apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
        })
        handler.postDelayed({
            runCatching {
                (getSystemService(ACTIVITY_SERVICE) as? ActivityManager)
                    ?.killBackgroundProcesses(DUCKSTATION_PACKAGE)
            }
        }, 450L)
        handler.postDelayed({
            runCatching {
                startActivity(Intent().apply {
                    setClassName(DUCKSTATION_PACKAGE, "$DUCKSTATION_PACKAGE.EmulationActivity")
                    action = Intent.ACTION_MAIN
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                    putExtra("bootPath", bootPath)
                    putExtra("EmulationActivity_bootPath", bootPath)
                })
            }.onFailure {
                Toast.makeText(this, "Restart the game from Rainey to apply ${selectedResolutionScale}x.", Toast.LENGTH_LONG).show()
            }
        }, 950L)
    }

    private fun removeOverlay() {
        val view = overlayView ?: return
        runCatching { windowManager()?.removeView(view) }
        overlayView = null
    }

    private fun params(
        width: Int,
        height: Int,
        gravity: Int = Gravity.CENTER
    ): WindowManager.LayoutParams {
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

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    companion object {
        private const val DUCKSTATION_PACKAGE = "com.github.stenzek.duckstation"
        private const val EXTRA_BOOT_PATH = "rainey_boot_path"
        private const val ACTION_SET_RESOLUTION = "com.rainey.frontend.duckstation.SET_RESOLUTION"
        private const val EXTRA_RESOLUTION_SCALE = "rainey_resolution_scale"

        fun startIfAllowed(context: android.content.Context, bootPath: String? = null): Boolean {
            if (!Settings.canDrawOverlays(context)) return false
            context.startService(Intent(context, DuckStationOverlayService::class.java).apply {
                putExtra(EXTRA_BOOT_PATH, bootPath)
            })
            return true
        }

        fun requestPermission(context: android.content.Context) {
            Toast.makeText(context, "Enable Display over other apps for Rainey pause menu.", Toast.LENGTH_LONG).show()
            context.startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION).apply {
                data = android.net.Uri.parse("package:${context.packageName}")
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            })
        }
    }
}
