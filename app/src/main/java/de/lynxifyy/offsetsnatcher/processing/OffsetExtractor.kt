package de.lynxifyy.offsetsnatcher.processing

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Thin wrapper around the native extraction library.
 *
 * The native side performs the full ELF parsing + pattern scanning and
 * returns a JSON document; loading a 100+ MB .so into memory and scanning it
 * happens on Dispatchers.IO so the UI thread is never blocked.
 */
object OffsetExtractor {

    private const val LIBRARY_NAME = "glowberry_extractor"

    init {
        System.loadLibrary(LIBRARY_NAME)
    }

    /** Loads `libminecraftpe.so` and returns the parsed [ExtractResult]. */
    suspend fun extract(soPath: String, meta: ApkMeta): ExtractResult =
        withContext(Dispatchers.IO) {
            val json = Bridge.extract(
                soPath = soPath,
                packageName = meta.packageName,
                versionName = meta.versionName,
                versionCode = meta.versionCode.toInt(),
            )
            ExtractResult.fromJson(json)
        }
}

/** JNI entry point — implemented in jni_bridge.cpp. */
internal object Bridge {
    external fun extract(
        soPath: String,
        packageName: String,
        versionName: String,
        versionCode: Int,
    ): String
}