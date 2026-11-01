package de.lynxifyy.offsetsnatcher

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import de.lynxifyy.offsetsnatcher.ui.screens.MainScreen
import de.lynxifyy.offsetsnatcher.ui.theme.GlowBerryTheme
import java.io.File
import java.util.UUID

class MainActivity : ComponentActivity() {

    private val viewModel: MainViewModel by viewModels()

    // File picker contract: a single */* (we filter to .apk by name).
    private val pickApk =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri == null) return@registerForActivityResult
            val name = queryDisplayName(uri) ?: "selected.apk"
            // ContentResolver URIs are not directly file-backed; copy into
            // cache so ZipFile/open can read it predictably.
            val cacheFile = File(cacheDir, "picked_${UUID.randomUUID()}.apk")
            try {
                contentResolver.openInputStream(uri)?.use { input ->
                    cacheFile.outputStream().use { output -> input.copyTo(output) }
                }
                viewModel.start(PickedApk(name = name, path = cacheFile.absolutePath))
            } catch (e: Exception) {
                viewModel.setStateError("Could not read the selected file: ${e.message}")
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        viewModel.attach(applicationContext)
        setContent {
            GlowBerryTheme {
                MainScreen(
                    viewModel = viewModel,
                    onPickApk = { pickApk.launch(arrayOf("*/*")) },
                )
            }
        }
    }

    private fun queryDisplayName(uri: android.net.Uri): String? =
        try {
            contentResolver.query(
                uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME),
                null, null, null
            )?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        } catch (_: Exception) {
            null
        }
}