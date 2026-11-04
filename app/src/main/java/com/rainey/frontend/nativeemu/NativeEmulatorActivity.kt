package com.rainey.frontend.nativeemu

import android.os.Bundle
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import android.widget.GridLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.lifecycle.lifecycleScope
import com.rainey.frontend.R
import com.rainey.frontend.settings.BorderStyles
import com.rainey.frontend.settings.SettingsStore
import com.swordfish.libretrodroid.GLRetroView
import com.swordfish.libretrodroid.GLRetroViewData
import com.swordfish.libretrodroid.LibretroDroid
import com.swordfish.libretrodroid.ShaderConfig
import java.io.File
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class NativeEmulatorActivity : ComponentActivity() {
    private var retroView: GLRetroView? = null
    private var rootFrame: FrameLayout? = null
    private var statusText: TextView? = null
    private var pauseOverlay: View? = null
    private var touchOverlayView: View? = null
    private var borderOverlayView: ImageView? = null
    private var frameRendered = false
    private var controllerLayout: String = "Nintendo"
    private var currentRomPath: String = ""
    private var currentSystemId: String = ""
    private var currentVideoScale: Int = 0
    private var borderStyle: String = BorderStyles.OFF
    private var touchControlsVisible: Boolean = false
    private var touchControlsOpacity: Float = 0.72f
    private val virtualDirections = mutableSetOf<Int>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.decorView.keepScreenOn = true

        val corePath = intent.getStringExtra(EXTRA_CORE_PATH)
        val romPath = intent.getStringExtra(EXTRA_ROM_PATH)
        val systemDir = intent.getStringExtra(EXTRA_SYSTEM_DIR) ?: filesDir.resolve("native_system").absolutePath
        val savesDir = filesDir.resolve("native_saves").apply { mkdirs() }.absolutePath
        controllerLayout = intent.getStringExtra(EXTRA_CONTROLLER_LAYOUT) ?: "Nintendo"
        touchControlsVisible = intent.getBooleanExtra(EXTRA_TOUCH_CONTROLS_ENABLED, false)
        touchControlsOpacity = intent.getFloatExtra(EXTRA_TOUCH_CONTROLS_OPACITY, 0.72f).coerceIn(0.35f, 1f)
        currentVideoScale = intent.getIntExtra(EXTRA_VIDEO_SCALE, 0).coerceIn(0, 4)
        borderStyle = intent.getStringExtra(EXTRA_BORDER_STYLE)?.takeIf { it in BorderStyles.options } ?: BorderStyles.OFF
        currentSystemId = intent.getStringExtra(EXTRA_SYSTEM_ID).orEmpty()

        if (corePath.isNullOrBlank() || romPath.isNullOrBlank()) {
            showError("Native emulator is missing core or ROM data.")
            return
        }
        currentRomPath = romPath

        val data = GLRetroViewData(this).apply {
            coreFilePath = corePath
            gameFilePath = romPath
            systemDirectory = systemDir
            savesDirectory = savesDir
            shader = ShaderConfig.Default
            rumbleEventsEnabled = true
            preferLowLatencyAudio = true
        }

        val root = FrameLayout(this).apply {
            setBackgroundColor(android.graphics.Color.BLACK)
        }
        rootFrame = root
        setContentView(root)
        retroView = GLRetroView(this, data)
        lifecycle.addObserver(retroView!!)
        root.addView(
            retroView,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            ).apply { gravity = Gravity.CENTER }
        )
        applyBorderOverlay(root)
        statusText = statusView("Loading native core...")
        root.addView(
            statusText,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            ).apply { gravity = Gravity.CENTER }
        )
        if (touchControlsVisible) {
            touchOverlayView = touchOverlay(touchControlsOpacity, currentSystemId)
            root.addView(touchOverlayView)
        }
        root.post { applyVideoScale(root, currentSystemId, currentVideoScale) }
        observeEmulator(root)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        retroView?.sendMotionEvent(GLRetroView.MOTION_SOURCE_DPAD, event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y))
        retroView?.sendMotionEvent(GLRetroView.MOTION_SOURCE_ANALOG_LEFT, event.getAxisValue(MotionEvent.AXIS_X), event.getAxisValue(MotionEvent.AXIS_Y))
        retroView?.sendMotionEvent(GLRetroView.MOTION_SOURCE_ANALOG_RIGHT, event.getAxisValue(MotionEvent.AXIS_Z), event.getAxisValue(MotionEvent.AXIS_RZ))
        return super.onGenericMotionEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (pauseOverlay != null) {
            return when (keyCode) {
                KeyEvent.KEYCODE_BACK,
                KeyEvent.KEYCODE_ESCAPE,
                KeyEvent.KEYCODE_MENU,
                KeyEvent.KEYCODE_BUTTON_B,
                KeyEvent.KEYCODE_BUTTON_MODE -> {
                    hidePauseMenu()
                    true
                }
                KeyEvent.KEYCODE_ENTER,
                KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_BUTTON_A -> {
                    currentFocus?.performClick()
                    true
                }
                KeyEvent.KEYCODE_DPAD_UP,
                KeyEvent.KEYCODE_DPAD_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT,
                KeyEvent.KEYCODE_DPAD_RIGHT -> super.onKeyDown(keyCode, event)
                else -> true
            }
        }
        if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE || keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_BUTTON_MODE) {
            togglePauseMenu()
            return true
        }
        if (keyCode == KeyEvent.KEYCODE_F5 || keyCode == KeyEvent.KEYCODE_BUTTON_THUMBL) {
            saveState()
            return true
        }
        if (keyCode == KeyEvent.KEYCODE_F8 || keyCode == KeyEvent.KEYCODE_BUTTON_THUMBR) {
            loadState()
            return true
        }
        retroView?.sendKeyEvent(event.action, mapControllerKey(keyCode))
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (pauseOverlay != null) return true
        retroView?.sendKeyEvent(event.action, mapControllerKey(keyCode))
        return true
    }

    private fun touchOverlay(opacity: Float, systemId: String): View {
        val root = FrameLayout(this)
        root.isClickable = false
        root.alpha = opacity

        val movement = if (systemId == "n64" || systemId == "arcade") {
            joystickView()
        } else {
            GridLayout(this).apply {
                rowCount = 3
                columnCount = 3
                addView(spacerButton())
                addView(controlButton("▲", KeyEvent.KEYCODE_DPAD_UP))
                addView(spacerButton())
                addView(controlButton("◀", KeyEvent.KEYCODE_DPAD_LEFT))
                addView(spacerButton())
                addView(controlButton("▶", KeyEvent.KEYCODE_DPAD_RIGHT))
                addView(spacerButton())
                addView(controlButton("▼", KeyEvent.KEYCODE_DPAD_DOWN))
                addView(spacerButton())
            }
        }
        root.addView(
            movement,
            FrameLayout.LayoutParams(dp(210), dp(210)).apply {
                gravity = Gravity.START or Gravity.BOTTOM
                leftMargin = dp(34)
                bottomMargin = dp(36)
            }
        )

        val face = GridLayout(this).apply {
            rowCount = 3
            columnCount = 3
            addView(spacerButton())
            addView(controlButton("X", KeyEvent.KEYCODE_BUTTON_X))
            addView(spacerButton())
            addView(controlButton("Y", KeyEvent.KEYCODE_BUTTON_Y))
            addView(spacerButton())
            addView(controlButton("A", KeyEvent.KEYCODE_BUTTON_A))
            addView(spacerButton())
            addView(controlButton("B", KeyEvent.KEYCODE_BUTTON_B))
            addView(spacerButton())
        }
        root.addView(
            face,
            FrameLayout.LayoutParams(dp(230), dp(230)).apply {
                gravity = Gravity.END or Gravity.BOTTOM
                rightMargin = dp(34)
                bottomMargin = dp(30)
            }
        )

        val shoulders = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            addView(controlButton("L", KeyEvent.KEYCODE_BUTTON_L1, wide = true))
            addView(actionButton("SAVE") { saveState() })
            addView(controlButton("SELECT", KeyEvent.KEYCODE_BUTTON_SELECT, wide = true))
            addView(controlButton("START", KeyEvent.KEYCODE_BUTTON_START, wide = true))
            addView(actionButton("LOAD") { loadState() })
            addView(controlButton("R", KeyEvent.KEYCODE_BUTTON_R1, wide = true))
        }
        root.addView(
            shoulders,
            FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, dp(68)).apply {
                gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
                topMargin = dp(20)
                leftMargin = dp(28)
                rightMargin = dp(28)
            }
        )

        return root
    }

    private fun joystickView(): View {
        return VirtualJoystickView(this).apply {
            layoutParams = FrameLayout.LayoutParams(dp(210), dp(210))
            onMove = { x, y ->
                retroView?.sendMotionEvent(GLRetroView.MOTION_SOURCE_ANALOG_LEFT, x, y)
                updateVirtualDpad(x, y)
            }
            onRelease = {
                retroView?.sendMotionEvent(GLRetroView.MOTION_SOURCE_ANALOG_LEFT, 0f, 0f)
                releaseVirtualDpad()
            }
        }
    }

    private fun togglePauseMenu() {
        if (pauseOverlay == null) showPauseMenu() else hidePauseMenu()
    }

    private fun showPauseMenu() {
        val root = rootFrame ?: return
        retroView?.frameSpeed = 0
        retroView?.audioEnabled = false

        pauseOverlay = FrameLayout(this).apply {
            setBackgroundColor(android.graphics.Color.argb(218, 2, 4, 12))
            isClickable = true
            isFocusable = true

            val panel = LinearLayout(this@NativeEmulatorActivity).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(dp(20), dp(12), dp(20), dp(12))
                background = roundedPanel(
                    fill = android.graphics.Color.argb(238, 8, 15, 31),
                    stroke = android.graphics.Color.argb(220, 33, 212, 253),
                    radiusDp = 14,
                    strokeDp = 2
                )

                addView(TextView(this@NativeEmulatorActivity).apply {
                    text = "RAINEY OPTIONS"
                    textSize = 22f
                    setTextColor(android.graphics.Color.WHITE)
                    gravity = Gravity.CENTER
                    typeface = android.graphics.Typeface.DEFAULT_BOLD
                }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(34)))

                addView(TextView(this@NativeEmulatorActivity).apply {
                    text = currentRomPath.substringAfterLast('/').substringBeforeLast('.').ifBlank { "Native game" }
                    textSize = 12f
                    setTextColor(android.graphics.Color.rgb(168, 190, 214))
                    gravity = Gravity.CENTER
                    maxLines = 1
                    ellipsize = android.text.TextUtils.TruncateAt.END
                }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(26)))

                addView(sectionLabel("Game"))
                addView(menuGrid(columns = 2).apply {
                    addView(menuButton("Resume", "Back to game") { hidePauseMenu() })
                    addView(menuButton("Reset", "Restart core") {
                        retroView?.reset()
                        hidePauseMenu()
                    })
                    addView(menuButton("Save", "State slot 1") { saveState() })
                    addView(menuButton("Load", "State slot 1") { loadState() })
                })

                addView(sectionLabel("Video Scale"))
                addView(menuGrid(columns = 5).apply {
                    addView(scaleButton("Auto", 0))
                    (1..4).forEach { addView(scaleButton("${it}x", it)) }
                })

                addView(sectionLabel("Screen Border"))
                addView(TextView(this@NativeEmulatorActivity).apply {
                    text = borderStyle
                    textSize = 12f
                    setTextColor(android.graphics.Color.rgb(168, 190, 214))
                    gravity = Gravity.CENTER
                    maxLines = 1
                    ellipsize = android.text.TextUtils.TruncateAt.END
                }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(20)))
                addView(menuGrid(columns = 3).apply {
                    BorderStyles.options.forEach { style ->
                        addView(borderButton(style))
                    }
                })

                addView(sectionLabel("Controls"))
                addView(menuGrid(columns = 2).apply {
                    addView(menuButton(
                        if (touchControlsVisible) "Touch Off" else "Touch On",
                        "Toggle overlay"
                    ) {
                        toggleTouchControls()
                        refreshPauseMenu()
                    })
                    addView(menuButton("Quit", "Return to Rainey") { finish() })
                })

                addView(TextView(this@NativeEmulatorActivity).apply {
                    text = "Shortcuts: L3/F5 save  •  R3/F8 load  •  Back/Menu options"
                    textSize = 11f
                    setTextColor(android.graphics.Color.rgb(168, 190, 214))
                    gravity = Gravity.CENTER
                    setPadding(0, dp(5), 0, 0)
                }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, dp(26)))
            }

            val scrollView = ScrollView(this@NativeEmulatorActivity).apply {
                isFillViewport = false
                isFocusable = true
                addView(
                    panel,
                    FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT
                    )
                )
            }

            addView(
                scrollView,
                FrameLayout.LayoutParams(dp(560), FrameLayout.LayoutParams.MATCH_PARENT).apply {
                    gravity = Gravity.CENTER
                    topMargin = dp(18)
                    bottomMargin = dp(18)
                }
            )
            post { findFocusTarget(this)?.requestFocus() }
        }
        root.addView(
            pauseOverlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        )
    }

    private fun hidePauseMenu() {
        val root = rootFrame
        pauseOverlay?.let { root?.removeView(it) }
        pauseOverlay = null
        retroView?.frameSpeed = 1
        retroView?.audioEnabled = true
    }

    private fun refreshPauseMenu() {
        val root = rootFrame ?: return
        pauseOverlay?.let { root.removeView(it) }
        pauseOverlay = null
        showPauseMenu()
    }

    private fun setVideoScale(scale: Int) {
        val root = rootFrame ?: return
        currentVideoScale = scale.coerceIn(0, 4)
        applyVideoScale(root, currentSystemId, currentVideoScale)
        Toast.makeText(this, if (currentVideoScale == 0) "Video scale: Auto" else "Video scale: ${currentVideoScale}x", Toast.LENGTH_SHORT).show()
        refreshPauseMenu()
    }

    private fun setBorderStyle(style: String) {
        val nextStyle = style.takeIf { it in BorderStyles.options } ?: BorderStyles.OFF
        borderStyle = nextStyle
        rootFrame?.let { applyBorderOverlay(it) }
        rootFrame?.let { applyVideoScale(it, currentSystemId, currentVideoScale) }
        lifecycleScope.launch {
            SettingsStore(applicationContext).setBorderStyle(nextStyle)
        }
        Toast.makeText(this, if (nextStyle == BorderStyles.OFF) "Screen border off" else "Screen border: $nextStyle", Toast.LENGTH_SHORT).show()
        refreshPauseMenu()
    }

    private fun cycleBorder(step: Int) {
        val currentIndex = BorderStyles.options.indexOf(borderStyle).takeIf { it >= 0 } ?: 0
        val nextIndex = (currentIndex + step + BorderStyles.options.size) % BorderStyles.options.size
        setBorderStyle(BorderStyles.options[nextIndex])
    }

    private fun toggleTouchControls() {
        val root = rootFrame ?: return
        touchOverlayView?.let { root.removeView(it) }
        touchOverlayView = null
        touchControlsVisible = !touchControlsVisible
        if (touchControlsVisible) {
            touchOverlayView = touchOverlay(touchControlsOpacity, currentSystemId)
            root.addView(touchOverlayView)
            pauseOverlay?.bringToFront()
        } else {
            releaseVirtualDpad()
        }
        Toast.makeText(this, if (touchControlsVisible) "Touch controls on" else "Touch controls off", Toast.LENGTH_SHORT).show()
    }

    private fun saveState() {
        val view = retroView ?: return
        runCatching {
            val state = view.serializeState()
            saveStateFile().apply { parentFile?.mkdirs() }.writeBytes(state)
        }.onSuccess {
            Toast.makeText(this, "State saved", Toast.LENGTH_SHORT).show()
        }.onFailure {
            Toast.makeText(this, "Save failed", Toast.LENGTH_SHORT).show()
        }
    }

    private fun loadState() {
        val view = retroView ?: return
        val file = saveStateFile()
        if (!file.exists()) {
            Toast.makeText(this, "No save state yet", Toast.LENGTH_SHORT).show()
            return
        }
        runCatching {
            view.unserializeState(file.readBytes())
        }.onSuccess { loaded ->
            Toast.makeText(this, if (loaded) "State loaded" else "Load failed", Toast.LENGTH_SHORT).show()
            if (loaded) hidePauseMenu()
        }.onFailure {
            Toast.makeText(this, "Load failed", Toast.LENGTH_SHORT).show()
        }
    }

    private fun saveStateFile(): File {
        val name = currentRomPath.substringAfterLast('/').ifBlank { "game" }
            .replace(Regex("[\\\\/:*?\"<>|]"), "_")
        return File(filesDir, "native_states/$name.state")
    }

    private fun controlButton(label: String, keyCode: Int, wide: Boolean = false): TextView {
        return TextView(this).apply {
            text = label
            textSize = if (wide) 13f else 22f
            setTextColor(android.graphics.Color.WHITE)
            gravity = Gravity.CENTER
            background = roundedPanel(
                fill = android.graphics.Color.argb(155, 18, 28, 48),
                stroke = android.graphics.Color.argb(145, 33, 212, 253),
                radiusDp = 14,
                strokeDp = 1
            )
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            val size = if (wide) LinearLayout.LayoutParams(0, dp(52), 1f) else ViewGroupMargin(dp(64), dp(64))
            layoutParams = size
            setOnTouchListener { _, event ->
                when (event.actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        sendTouchKey(KeyEvent.ACTION_DOWN, keyCode)
                        isPressed = true
                        true
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        sendTouchKey(KeyEvent.ACTION_UP, keyCode)
                        isPressed = false
                        true
                    }
                    else -> true
                }
            }
        }
    }

    private fun actionButton(label: String, onClick: () -> Unit): TextView {
        return TextView(this).apply {
            text = label
            textSize = 13f
            setTextColor(android.graphics.Color.WHITE)
            gravity = Gravity.CENTER
            background = roundedPanel(
                fill = android.graphics.Color.argb(155, 18, 28, 48),
                stroke = android.graphics.Color.argb(145, 184, 59, 255),
                radiusDp = 14,
                strokeDp = 1
            )
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, dp(52), 1f)
            setOnClickListener { onClick() }
        }
    }

    private fun menuGrid(columns: Int): GridLayout {
        return GridLayout(this).apply {
            columnCount = columns
            rowCount = 1
            layoutParams = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
        }
    }

    private fun sectionLabel(label: String): TextView {
        return TextView(this).apply {
            text = label.uppercase()
            textSize = 11f
            letterSpacing = 0.08f
            setTextColor(android.graphics.Color.rgb(33, 212, 253))
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setPadding(0, dp(7), 0, dp(3))
        }
    }

    private fun scaleButton(label: String, scale: Int): TextView {
        return menuButton(label, if (scale == currentVideoScale) "Active" else "Scale") {
            setVideoScale(scale)
        }.apply {
            if (scale == currentVideoScale) {
                background = roundedPanel(
                    fill = android.graphics.Color.argb(225, 14, 75, 96),
                    stroke = android.graphics.Color.argb(245, 33, 212, 253),
                    radiusDp = 10,
                    strokeDp = 2
                )
            }
        }
    }

    private fun borderButton(style: String): TextView {
        return menuButton(style, if (style == borderStyle) "Active" else "") {
            setBorderStyle(style)
        }.apply {
            if (style == borderStyle) {
                background = roundedPanel(
                    fill = android.graphics.Color.argb(225, 14, 75, 96),
                    stroke = android.graphics.Color.argb(245, 33, 212, 253),
                    radiusDp = 10,
                    strokeDp = 2
                )
            }
        }
    }

    private fun menuButton(label: String, subtitle: String = "", onClick: () -> Unit): TextView {
        return TextView(this).apply {
            text = if (subtitle.isBlank()) label else "$label\n$subtitle"
            textSize = 15f
            setTextColor(android.graphics.Color.WHITE)
            gravity = Gravity.CENTER
            setPadding(dp(8), dp(4), dp(8), dp(4))
            background = roundedPanel(
                fill = android.graphics.Color.argb(205, 14, 29, 54),
                stroke = android.graphics.Color.argb(180, 33, 212, 253),
                radiusDp = 10,
                strokeDp = 1
            )
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            isFocusable = true
            isClickable = true
            layoutParams = GridLayout.LayoutParams().apply {
                width = 0
                height = dp(50)
                columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f)
                setMargins(dp(4), dp(4), dp(4), dp(4))
            }
            setOnClickListener { onClick() }
        }
    }

    private fun findFocusTarget(view: View): View? {
        if (view.isFocusable && view.isClickable) return view
        if (view is android.view.ViewGroup) {
            for (index in 0 until view.childCount) {
                findFocusTarget(view.getChildAt(index))?.let { return it }
            }
        }
        return null
    }

    private fun roundedPanel(fill: Int, stroke: Int, radiusDp: Int, strokeDp: Int): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            cornerRadius = dp(radiusDp).toFloat()
            setColor(fill)
            setStroke(dp(strokeDp), stroke)
        }
    }

    private fun spacerButton(): View {
        return View(this).apply {
            layoutParams = ViewGroupMargin(dp(64), dp(64))
        }
    }

    private fun sendTouchKey(action: Int, keyCode: Int) {
        retroView?.sendKeyEvent(action, mapControllerKey(keyCode))
    }

    private fun updateVirtualDpad(x: Float, y: Float) {
        val next = mutableSetOf<Int>()
        val threshold = 0.42f
        if (x < -threshold) next += KeyEvent.KEYCODE_DPAD_LEFT
        if (x > threshold) next += KeyEvent.KEYCODE_DPAD_RIGHT
        if (y < -threshold) next += KeyEvent.KEYCODE_DPAD_UP
        if (y > threshold) next += KeyEvent.KEYCODE_DPAD_DOWN

        (next - virtualDirections).forEach { sendTouchKey(KeyEvent.ACTION_DOWN, it) }
        (virtualDirections - next).forEach { sendTouchKey(KeyEvent.ACTION_UP, it) }
        virtualDirections.clear()
        virtualDirections.addAll(next)
    }

    private fun releaseVirtualDpad() {
        virtualDirections.forEach { sendTouchKey(KeyEvent.ACTION_UP, it) }
        virtualDirections.clear()
    }

    private fun mapControllerKey(keyCode: Int): Int {
        if (controllerLayout != "Xbox") return keyCode
        return when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_B -> KeyEvent.KEYCODE_BUTTON_A
            KeyEvent.KEYCODE_BUTTON_A -> KeyEvent.KEYCODE_BUTTON_B
            KeyEvent.KEYCODE_BUTTON_X -> KeyEvent.KEYCODE_BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_Y -> KeyEvent.KEYCODE_BUTTON_X
            else -> keyCode
        }
    }

    private fun applyVideoScale(root: FrameLayout, systemId: String, scale: Int) {
        val view = retroView ?: return
        val borderWindow = activeBorderWindow()
        if (borderWindow != null && root.width > 0 && root.height > 0) {
            val windowAspect = ((borderWindow.width() * root.width) / (borderWindow.height() * root.height))
                .takeIf { it > 0f } ?: (4f / 3f)
            view.setAspectRatio(windowAspect)
            view.viewport = borderWindow
            return
        }
        if (scale <= 0 || root.width <= 0 || root.height <= 0) {
            view.resetAspectRatio()
            view.viewport = RectF(0f, 0f, 1f, 1f)
            return
        }

        val baseSize = baseResolutionFor(systemId)
        val fitScale = minOf(
            root.width.toFloat() / baseSize.first.toFloat(),
            root.height.toFloat() / baseSize.second.toFloat()
        )
        if (scale > 1 && (fitScale >= scale || scale >= 4)) {
            view.setAspectRatio(root.width.toFloat() / root.height.toFloat())
            view.viewport = RectF(0f, 0f, 1f, 1f)
            return
        }

        view.resetAspectRatio()
        val targetWidth = baseSize.first * scale
        val targetHeight = baseSize.second * scale
        val viewportWidth = (targetWidth.toFloat() / root.width.toFloat()).coerceAtMost(1f)
        val viewportHeight = (targetHeight.toFloat() / root.height.toFloat()).coerceAtMost(1f)
        val left = (1f - viewportWidth) / 2f
        val top = (1f - viewportHeight) / 2f
        view.viewport = RectF(left, top, left + viewportWidth, top + viewportHeight)
    }

    private fun baseResolutionFor(systemId: String): Pair<Int, Int> {
        return when (systemId) {
            "nes" -> 256 to 240
            "snes" -> 256 to 224
            "genesis" -> 320 to 224
            "playstation" -> 320 to 240
            "gb" -> 160 to 144
            "gba" -> 240 to 160
            "n64" -> 320 to 240
            "arcade" -> 320 to 240
            "dreamcast" -> 640 to 480
            "psp" -> 480 to 272
            else -> 320 to 240
        }
    }

    private fun showError(message: String) {
        val text = statusView(message)
        setContentView(text)
    }

    private fun observeEmulator(root: FrameLayout) {
        val view = retroView ?: return
        lifecycleScope.launch {
            view.getGLRetroEvents().collect { event ->
                if (event == GLRetroView.GLRetroEvents.FrameRendered) {
                    frameRendered = true
                    statusText?.let { root.removeView(it) }
                    statusText = null
                }
            }
        }
        lifecycleScope.launch {
            view.getGLRetroErrors().collect { errorCode ->
                val message = when (errorCode) {
                    LibretroDroid.ERROR_LOAD_LIBRARY -> "Core failed to load. Check that the selected core is an Android arm64 libretro .so file."
                    LibretroDroid.ERROR_LOAD_GAME -> "Game failed to load. Check the ROM format, BIOS/system files, and that the selected core supports this file."
                    LibretroDroid.ERROR_GL_NOT_COMPATIBLE -> "This display/GPU OpenGL mode is not compatible with the selected core."
                    else -> "Native emulator error $errorCode. Try another core or ROM format."
                }
                showOverlayMessage(root, message, removeSurface = true)
            }
        }
        lifecycleScope.launch {
            delay(10_000)
            if (!frameRendered) {
                showOverlayMessage(
                    root,
                    "Still waiting for video. The core may not support this ROM, may need BIOS files, or may not be compatible with Android arm64.",
                    removeSurface = true
                )
            }
        }
    }

    private fun showOverlayMessage(root: FrameLayout, message: String, removeSurface: Boolean) {
        if (removeSurface) {
            retroView?.let {
                root.removeView(it)
                lifecycle.removeObserver(it)
            }
            retroView = null
        }
        if (statusText == null) {
            statusText = statusView(message)
            root.addView(
                statusText,
                FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                ).apply { gravity = Gravity.CENTER }
            )
        } else {
            statusText?.text = message
        }
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
    }

    private fun statusView(message: String): TextView {
        return TextView(this).apply {
            text = message
            textSize = 20f
            setTextColor(android.graphics.Color.WHITE)
            gravity = Gravity.CENTER
            setPadding(40, 40, 40, 40)
        }
    }

    private fun applyBorderOverlay(root: FrameLayout) {
        borderOverlayView?.let { root.removeView(it) }
        borderOverlayView = null
        val drawableId = borderDrawableFor(borderStyle) ?: return
        val overlay = ImageView(this).apply {
            setImageResource(drawableId)
            scaleType = ImageView.ScaleType.FIT_XY
            alpha = 0.96f
            isClickable = false
            isFocusable = false
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
        }
        borderOverlayView = overlay
        root.addView(
            overlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        )
        touchOverlayView?.bringToFront()
        statusText?.bringToFront()
        pauseOverlay?.bringToFront()
    }

    private fun activeBorderWindow(): RectF? {
        return borderWindowFor(borderStyle)
    }

    private fun borderWindowFor(style: String): RectF? {
        return when (style) {
            "CRT Classic" -> RectF(0.10f, 0.30f, 0.90f, 0.91f)
            "Arcade Bezel" -> RectF(0.08f, 0.11f, 0.91f, 0.80f)
            "8-Bit Pixel" -> RectF(0.08f, 0.12f, 0.92f, 0.84f)
            "Game Boy" -> RectF(0.11f, 0.16f, 0.85f, 0.79f)
            "SNES" -> RectF(0.12f, 0.12f, 0.88f, 0.85f)
            "Mega Drive" -> RectF(0.08f, 0.11f, 0.92f, 0.86f)
            "Neon Grid" -> RectF(0.08f, 0.11f, 0.92f, 0.86f)
            "Woodgrain TV" -> RectF(0.08f, 0.10f, 0.79f, 0.85f)
            "Comic Book" -> RectF(0.10f, 0.13f, 0.90f, 0.87f)
            "Minimal Dark" -> RectF(0.08f, 0.12f, 0.92f, 0.88f)
            "Vaporwave" -> RectF(0.08f, 0.12f, 0.92f, 0.88f)
            "Golden Age" -> RectF(0.08f, 0.10f, 0.92f, 0.88f)
            "Space HUD" -> RectF(0.05f, 0.09f, 0.95f, 0.84f)
            "CRT Terminal" -> RectF(0.05f, 0.10f, 0.96f, 0.83f)
            "Industrial Metal" -> RectF(0.08f, 0.11f, 0.90f, 0.81f)
            "Crystal Purple" -> RectF(0.08f, 0.11f, 0.92f, 0.84f)
            "Steampunk" -> RectF(0.10f, 0.13f, 0.78f, 0.82f)
            "Handheld Minimal" -> RectF(0.11f, 0.10f, 0.87f, 0.88f)
            "Cyberpunk" -> RectF(0.11f, 0.14f, 0.88f, 0.82f)
            "Art Nouveau" -> RectF(0.10f, 0.12f, 0.90f, 0.83f)
            "16-Bit Landscape" -> RectF(0.13f, 0.12f, 0.86f, 0.78f)
            "Synthwave Sunset" -> RectF(0.11f, 0.12f, 0.90f, 0.78f)
            "Retro Grid 2" -> RectF(0.07f, 0.17f, 0.93f, 0.84f)
            "Horror Gothic" -> RectF(0.11f, 0.13f, 0.90f, 0.81f)
            else -> null
        }
    }

    private fun borderDrawableFor(style: String): Int? {
        return when (style) {
            "CRT Classic" -> R.drawable.border_crt_classic
            "Arcade Bezel" -> R.drawable.border_arcade_bezel
            "8-Bit Pixel" -> R.drawable.border_8_bit_pixel
            "Game Boy" -> R.drawable.border_game_boy
            "SNES" -> R.drawable.border_snes
            "Mega Drive" -> R.drawable.border_mega_drive
            "Neon Grid" -> R.drawable.border_neon_grid
            "Woodgrain TV" -> R.drawable.border_woodgrain_tv
            "Comic Book" -> R.drawable.border_comic_book
            "Minimal Dark" -> R.drawable.border_minimal_dark
            "Vaporwave" -> R.drawable.border_vaporwave
            "Golden Age" -> R.drawable.border_golden_age
            "Space HUD" -> R.drawable.border_space_hud
            "CRT Terminal" -> R.drawable.border_crt_terminal
            "Industrial Metal" -> R.drawable.border_industrial_metal
            "Crystal Purple" -> R.drawable.border_crystal_purple
            "Steampunk" -> R.drawable.border_steampunk
            "Handheld Minimal" -> R.drawable.border_handheld_minimal
            "Cyberpunk" -> R.drawable.border_cyberpunk
            "Art Nouveau" -> R.drawable.border_art_nouveau
            "16-Bit Landscape" -> R.drawable.border_16_bit_landscape
            "Synthwave Sunset" -> R.drawable.border_synthwave_sunset
            "Retro Grid 2" -> R.drawable.border_retro_grid_2
            "Horror Gothic" -> R.drawable.border_horror_gothic
            else -> null
        }
    }

    companion object {
        const val EXTRA_CORE_PATH = "com.rainey.frontend.nativeemu.CORE_PATH"
        const val EXTRA_ROM_PATH = "com.rainey.frontend.nativeemu.ROM_PATH"
        const val EXTRA_SYSTEM_DIR = "com.rainey.frontend.nativeemu.SYSTEM_DIR"
        const val EXTRA_TOUCH_CONTROLS_ENABLED = "com.rainey.frontend.nativeemu.TOUCH_CONTROLS_ENABLED"
        const val EXTRA_TOUCH_CONTROLS_OPACITY = "com.rainey.frontend.nativeemu.TOUCH_CONTROLS_OPACITY"
        const val EXTRA_CONTROLLER_LAYOUT = "com.rainey.frontend.nativeemu.CONTROLLER_LAYOUT"
        const val EXTRA_VIDEO_SCALE = "com.rainey.frontend.nativeemu.VIDEO_SCALE"
        const val EXTRA_BORDER_STYLE = "com.rainey.frontend.nativeemu.BORDER_STYLE"
        const val EXTRA_SYSTEM_ID = "com.rainey.frontend.nativeemu.SYSTEM_ID"
    }
}

private fun NativeEmulatorActivity.dp(value: Int): Int {
    return (value * resources.displayMetrics.density).toInt()
}

private fun ViewGroupMargin(width: Int, height: Int): GridLayout.LayoutParams {
    return GridLayout.LayoutParams().apply {
        this.width = width
        this.height = height
        setMargins(5, 5, 5, 5)
    }
}

private class VirtualJoystickView(context: android.content.Context) : View(context) {
    var onMove: ((Float, Float) -> Unit)? = null
    var onRelease: (() -> Unit)? = null
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var stickX = 0f
    private var stickY = 0f

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val radius = minOf(width, height) * 0.42f
        val knobRadius = minOf(width, height) * 0.16f
        val cx = width / 2f
        val cy = height / 2f
        paint.style = Paint.Style.FILL
        paint.color = android.graphics.Color.argb(92, 33, 212, 253)
        canvas.drawCircle(cx, cy, radius, paint)
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = 4f
        paint.color = android.graphics.Color.argb(190, 33, 212, 253)
        canvas.drawCircle(cx, cy, radius, paint)
        paint.style = Paint.Style.FILL
        paint.color = android.graphics.Color.argb(195, 244, 247, 251)
        canvas.drawCircle(cx + stickX * radius, cy + stickY * radius, knobRadius, paint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                val radius = minOf(width, height) * 0.42f
                val dx = ((event.x - width / 2f) / radius).coerceIn(-1f, 1f)
                val dy = ((event.y - height / 2f) / radius).coerceIn(-1f, 1f)
                val distance = kotlin.math.hypot(dx.toDouble(), dy.toDouble()).toFloat()
                if (distance > 1f) {
                    stickX = dx / distance
                    stickY = dy / distance
                } else {
                    stickX = dx
                    stickY = dy
                }
                onMove?.invoke(stickX, stickY)
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                stickX = 0f
                stickY = 0f
                onRelease?.invoke()
                invalidate()
            }
        }
        return true
    }
}
