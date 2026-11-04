package com.rainey.frontend

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import com.rainey.frontend.ui.RaineyApp
import com.rainey.frontend.ui.RaineyViewModel
import com.rainey.frontend.ui.RaineyViewModelFactory
import com.rainey.frontend.ui.theme.RaineyTheme

class MainActivity : ComponentActivity() {
    private enum class FolderPickTarget { Rom, Cores, Bios }
    private var activityVisible = false

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_F10 || event.keyCode == KeyEvent.KEYCODE_F12) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                closeVisibleRaineyFromQuickButton("activity-key-down")
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        closeVisibleRaineyFromQuickButton("direct-launch")
    }

    override fun onResume() {
        super.onResume()
        activityVisible = true
        LauncherButtonRouter.markRaineyResumed()
    }

    override fun onPause() {
        activityVisible = false
        LauncherButtonRouter.markRaineyPaused()
        super.onPause()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as RaineyApplication
        setContent {
            RaineyTheme {
                RaineyRoot(app)
            }
        }
    }

    @Composable
    private fun RaineyRoot(app: RaineyApplication) {
        val viewModel: RaineyViewModel = viewModel(
            factory = RaineyViewModelFactory(
                application = app,
                repository = app.repository,
                settingsStore = app.settingsStore
            )
        )
        var pendingSystemId by remember { mutableStateOf<String?>(null) }
        var pendingFolderPickTarget by remember { mutableStateOf(FolderPickTarget.Rom) }
        var pendingContentSystemId by remember { mutableStateOf<String?>(null) }
        val folderPicker = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocumentTree()
        ) { uri: Uri? ->
            if (uri != null) {
                contentResolver.takePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
                when (pendingFolderPickTarget) {
                    FolderPickTarget.Rom -> {
                        val systemId = pendingSystemId
                        if (systemId != null) viewModel.addRomFolder(systemId, uri)
                    }
                    FolderPickTarget.Cores -> viewModel.setRetroArchCoresFolder(uri)
                    FolderPickTarget.Bios -> viewModel.setBiosFolder(uri)
                }
            }
            pendingSystemId = null
            pendingFolderPickTarget = FolderPickTarget.Rom
        }
        val contentPicker = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri: Uri? ->
            val systemId = pendingContentSystemId
            if (uri != null && systemId != null) {
                contentResolver.takePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
                viewModel.addManualGame(systemId, uri)
            }
            pendingContentSystemId = null
        }
        val imagePermission = if (Build.VERSION.SDK_INT >= 33) {
            Manifest.permission.READ_MEDIA_IMAGES
        } else {
            Manifest.permission.READ_EXTERNAL_STORAGE
        }
        val imagePermissionLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.RequestPermission()
        ) { granted ->
            if (granted) {
                viewModel.importArtworkForSelectedSystem()
            } else {
                viewModel.showStatus("Photos permission is needed to import artwork from Downloads or Pictures.")
            }
        }

        LaunchedEffect(Unit) {
            viewModel.bootstrap()
        }

        RaineyApp(
            viewModel = viewModel,
            onPickRomFolder = { systemId ->
                pendingSystemId = systemId
                pendingFolderPickTarget = FolderPickTarget.Rom
                folderPicker.launch(null)
            },
            onPickCoresFolder = {
                pendingSystemId = null
                pendingFolderPickTarget = FolderPickTarget.Cores
                folderPicker.launch(null)
            },
            onPickBiosFolder = {
                pendingSystemId = null
                pendingFolderPickTarget = FolderPickTarget.Bios
                folderPicker.launch(null)
            },
            onLoadContent = { systemId ->
                pendingContentSystemId = systemId
                contentPicker.launch(arrayOf("*/*"))
            },
            onImportArtwork = {
                if (ContextCompat.checkSelfPermission(this, imagePermission) == PackageManager.PERMISSION_GRANTED) {
                    viewModel.importArtworkForSelectedSystem()
                } else {
                    imagePermissionLauncher.launch(imagePermission)
                }
            }
        )
    }

    private fun closeVisibleRaineyFromQuickButton(source: String) {
        Log.i(TAG, "Closing Rainey from quick launcher source=$source visible=$activityVisible")
        LauncherButtonRouter.openAndroidHome(this)
        LauncherButtonRouter.markRaineyPaused()
        activityVisible = false
        finishAndRemoveTask()
    }

    private companion object {
        const val TAG = "RaineyMainActivity"
    }
}
