package com.rainey.frontend

import android.app.Activity
import android.os.Bundle

class HomeToggleActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        LauncherButtonRouter.handleHomePressed(this)
        finish()
    }
}
