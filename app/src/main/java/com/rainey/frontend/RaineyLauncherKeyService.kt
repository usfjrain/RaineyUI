package com.rainey.frontend

import android.accessibilityservice.AccessibilityService
import android.util.Log
import android.view.KeyEvent
import android.view.accessibility.AccessibilityEvent

class RaineyLauncherKeyService : AccessibilityService() {
    override fun onKeyEvent(event: KeyEvent): Boolean {
        Log.i(
            TAG,
            "keyCode=${event.keyCode} action=${event.action} repeat=${event.repeatCount} device=${event.device?.name ?: "unknown"}"
        )
        if (event.keyCode in quickLauncherKeys) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                LauncherButtonRouter.toggle(this, source = "accessibility-down")
            }
            return true
        }
        return false
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) = Unit

    override fun onInterrupt() = Unit

    private companion object {
        const val TAG = "RaineyLauncherKey"
        val quickLauncherKeys = setOf(KeyEvent.KEYCODE_F10, KeyEvent.KEYCODE_F12)
    }
}
