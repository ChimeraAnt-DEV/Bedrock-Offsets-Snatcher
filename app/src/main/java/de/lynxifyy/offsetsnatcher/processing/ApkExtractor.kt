package de.lynxifyy.offsetsnatcher.processing

import android.content.Context
import android.system.Os
import de.lynxifyy.offsetsnatcher.util.rwOwnerOnly
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.BufferedInputStream
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.util.zip.ZipFile

/**
 * Phase 1: APK processing & extraction.
 *
 * Opens the selected APK as a zip, locates the arm64-v8a `libminecraftpe.so`,
 * extracts it into app-private storage and reads the Minecraft version.
 * Everything runs on Dispatchers.IO; nothing touches the network.
 */
class ApkExtractor(private val context: Context) {

    /** Result of the APK staging step. */
    data class StagedApk(
        val apkPath: String,
        val soPath: String,
        val meta: ApkMeta,
    )

    /**
     * Extract `lib/arm64-v8a/libminecraftpe.so` plus version info.
     *
     * @param apkPath absolute path to the selected .apk file.
     */
    suspend fun stage(apkPath: String): StagedApk = withContext(Dispatchers.IO) {
        val apk = File(apkPath)
        require(apk.isFile && apk.length() > 0) { "Selected file is not a readable APK" }

        val targetAgar = "lib/arm64-v8a/libminecraftpe.so"

        // Resolve version meta first (tiny read, also used for early errors).
        val meta = readMeta(apkPath)

        ZipFile(apkPath).use { zip ->
            val entry = zip.getEntry(targetAgar)
                ?: run {
                    // Some APKs use the 'lib/' root without an ABI subfolder.
                    zip.getEntry("lib/libminecraftpe.so")
                }
                ?: throw IllegalArgumentException(
                    "No lib/arm64-v8a/libminecraftpe.so found in the APK. " +
                        "Only Minecraft Bedrock (arm64-v8a) APKs are supported."
                )

            val private = File(context.filesDir, "glowberry")
            private.mkdirs()

            val dest = File(private, "libminecraftpe.so")
            copyEntry(zip.getInputStream(entry), dest)

            StagedApk(
                apkPath = apkPath,
                soPath = dest.absolutePath,
                meta = meta,
            )
        }
    }

    private fun copyEntry(input: InputStream, dest: File): File {
        dest.parentFile?.mkdirs()
        FileOutputStream(dest).use { out ->
            BufferedInputStream(input, 64 * 1024).use { ins ->
                ins.copyTo(out, 64 * 1024)
            }
        }
        // Make sure reads from native code don't trip over anything.
        Os.chmod(dest.absolutePath, rwOwnerOnly)
        return dest
    }

    /** Parse version metadata. Prefers AndroidManifest.xml, falls back to text/JSON files. */
    private fun readMeta(apkPath: String): ApkMeta = try {
        ManifestParser.parse(apkPath)
    } catch (_: Exception) {
        // Fallback: version.txt / version.json inside assets if present.
        fallbackMeta(apkPath) ?: ApkMeta("unknown", "unknown", 0)
    }

    private fun fallbackMeta(apkPath: String): ApkMeta? {
        ZipFile(apkPath).use { zip ->
            for (name in listOf("version.txt", "assets/version.txt", "version.json")) {
                val entry = zip.getEntry(name) ?: continue
                val text = zip.getInputStream(entry).bufferedReader().use { it.readText().trim() }
                if (text.isNotEmpty()) {
                    val version = text.lineSequence()
                        .map { it.trim() }
                        .firstOrNull { it.isNotEmpty() } ?: return null
                    return ApkMeta(
                        packageName = "com.mojang.minecraftpe",
                        versionName = version.split(":").last().trim(),
                        versionCode = 0, // not present in version.txt
                    )
                }
            }
        }
        return null
    }
}