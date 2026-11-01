package de.lynxifyy.offsetsnatcher

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import de.lynxifyy.offsetsnatcher.processing.ApkExtractor
import de.lynxifyy.offsetsnatcher.processing.ApkMeta
import de.lynxifyy.offsetsnatcher.processing.ExtractResult
import de.lynxifyy.offsetsnatcher.processing.OffsetExtractor
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File

/** One state of the extraction pipeline. */
enum class ExtractPhase(val label: String) {
    IDLE("Ready"),
    EXTRACTING("Extracting APK…"),
    PARSING("Parsing ELF…"),
    SCANNING("Scanning signatures…"),
    DONE("Done"),
    ERROR("Error"),
}

data class PipelineState(
    val phase: ExtractPhase = ExtractPhase.IDLE,
    val progress: Float = 0f,
    val message: String = "",
    val jsonOutput: String? = null,
)

data class PickedApk(
    val name: String,
    val path: String,
)

class MainViewModel : ViewModel() {

    private val _state = MutableStateFlow(PipelineState())
    val state: StateFlow<PipelineState> = _state.asStateFlow()

    private var contextRef: android.content.Context? = null

    fun attach(context: android.content.Context) {
        if (contextRef == null) contextRef = context.applicationContext
    }

    /** Clears the output and resets to the idle state. */
    fun reset() {
        _state.value = PipelineState()
    }

    /** Pushes a user-visible error (e.g. failed file read). */
    fun setStateError(message: String) {
        _state.value = PipelineState(
            phase = ExtractPhase.ERROR,
            progress = 1f,
            message = message,
        )
    }

    /**
     * Run the pipeline on [picked]: extract the APK, scan the native library
     * and fill in the resulting JSON.
     */
    fun start(picked: PickedApk) {
        val context = contextRef ?: run {
            _state.value = PipelineState(
                phase = ExtractPhase.ERROR,
                message = "Context is not attached (internal error).",
            )
            return
        }
        viewModelScope.launch {
            try {
                // Phase 1: APK extraction.
                _state.value = _state.value.copy(
                    phase = ExtractPhase.EXTRACTING,
                    progress = 0.05f,
                    message = "Reading ${File(picked.path).name}",
                )
                val extractor = ApkExtractor(context)
                val staged = extractor.stage(picked.path)
                _state.value = _state.value.copy(
                    progress = 0.45f,
                    message = "Found ${staged.meta.versionName} (arm64-v8a)",
                )

                // Phase 2+3: native ELF parsing + scanning.
                _state.value = _state.value.copy(
                    phase = ExtractPhase.PARSING,
                    progress = 0.5f,
                    message = "Parsing ELF…",
                )
                val result = OffsetExtractor.extract(staged.soPath, staged.meta)

                _state.value = _state.value.copy(
                    phase = ExtractPhase.SCANNING,
                    progress = 0.85f,
                    message = if (result.ok) {
                        "Found ${result.functions.size} function / " +
                            "${result.vtables.size} vtable offset(s)"
                    } else {
                        result.error ?: "Extraction failed"
                    },
                )

                if (!result.ok) {
                    _state.value = _state.value.copy(
                        phase = ExtractPhase.ERROR,
                        progress = 1f,
                        message = result.error ?: "Extraction failed",
                        jsonOutput = result.toJson(),
                    )
                    return@launch
                }

                _state.value = _state.value.copy(
                    phase = ExtractPhase.DONE,
                    progress = 1f,
                    message = "Ready — copy or export your JSON",
                    jsonOutput = result.toJson(),
                )
            } catch (e: Exception) {
                _state.value = PipelineState(
                    phase = ExtractPhase.ERROR,
                    progress = 1f,
                    message = e.message ?: e.javaClass.simpleName,
                )
            }
        }
    }
}