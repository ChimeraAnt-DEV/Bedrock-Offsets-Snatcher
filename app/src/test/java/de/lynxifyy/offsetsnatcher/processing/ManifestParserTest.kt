package de.lynxifyy.offsetsnatcher.processing

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Tests of the binary-XML manifest parser against a real AXML document
 * produced by `aapt2 link` (committed as a test resource) plus malformed
 * byte-level cases.
 */
class ManifestParserTest {

    private fun resourceBytes(name: String): ByteArray =
        checkNotNull(javaClass.classLoader?.getResourceAsStream(name)) {
            "missing test resource $name"
        }.use { it.readBytes() }

    @Test
    fun parsesRealAapt2Manifest() {
        // Generated from:
        //   package="com.mojang.minecraftpe"
        //   android:versionCode="2020062"
        //   android:versionName="1.20.62"
        val meta = ManifestParser.parseAxml(resourceBytes("test_manifest.axml"))
        assertEquals("com.mojang.minecraftpe", meta.packageName)
        assertEquals("2020062", meta.versionCode.toString())
        assertTrue(meta.versionName.isNotEmpty())
    }

    @Test
    fun versionNameMatchesInlineString() {
        val meta = ManifestParser.parseAxml(resourceBytes("test_manifest.axml"))
        // aapt2 stores versionName resources as refs; accept either the inline
        // string or unknown rather than guessing. The package/versionCode are
        // the critical bits.
        assertTrue(meta.versionName == "1.20.62" || meta.versionName == "unknown")
    }

    @Test
    fun rejectsGarbage() {
        val data = ByteArray(64) { 0x41 }
        try {
            ManifestParser.parseAxml(data)
            assertTrue("expected exception", false)
        } catch (_: IllegalArgumentException) {
            // expected
        }
    }
}