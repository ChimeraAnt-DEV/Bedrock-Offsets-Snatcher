package de.lynxifyy.offsetsnatcher.processing

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets
import java.util.zip.ZipFile

/**
 * Parser for Android's binary XML ("AXML") format — exactly the parts of
 * AndroidManifest.xml we need:
 *
 *   - the `package` attribute of the `<manifest>` element
 *   - `android:versionName` (an inline UTF-8/UTF-16 string when present)
 *   - `android:versionCode` (a reference-id-followed integer)
 *
 * AXML chunk summary (all little-endian):
 *
 *   <8 bytes: chunk header>
 *     u16 type, u16 headerSize, u32 size          -- total chunk size
 *
 *   String pool (type 0x0001):
 *     u32 stringCount, u32 styleCount, u32 flags, u32 stringsStart,
 *     u32 stylesStart, u32 stringOffsets[stringCount],
 *     u32 styleOffsets[styleCount], <string blob>, <style blob>
 *
 *   Element (type 0x0102):
 *     u32 lineNumber, u32 comment, u32 ns, u32 name,
 *     u16 attributeStart, u16 attributeSize, u16 attributeCount,
 *     u16 idIndex, u16 classIndex, <attribute block>
 *
 *   Resource map (type 0x0180): array of u32 resource ids that apply to the
 *   immediately following element's attributes in order.
 *
 * When `android:versionName` is a resource reference (dataType 0x01) the
 * inline value does not exist and "unknown" is returned; the caller then
 * falls back to version.txt.
 */
object ManifestParser {

    private const val CHUNK_STRING_POOL = 0x0001
    private const val CHUNK_START_ELEMENT = 0x0102

    private const val TYPE_STRING = 0x03
    private const val TYPE_INT_DEC = 0x10
    private const val TYPE_INT_HEX = 0x11

    private const val UTF8_FLAG = 0x100

    fun parse(apkPath: String): ApkMeta {
        ZipFile(apkPath).use { zip ->
            val entry = zip.getEntry("AndroidManifest.xml")
                ?: throw IllegalArgumentException("APK has no AndroidManifest.xml")
            val bytes = zip.getInputStream(entry).use { it.readBytes() }
            return parseAxml(bytes)
        }
    }

    internal fun parseAxml(data: ByteArray): ApkMeta {
        val buf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)

        // XML header: type == 0x0003.
        if (buf.remaining() < 8) throw IllegalArgumentException("empty manifest")
        val headerType = buf.short.toInt() and 0xffff
        require(headerType == 0x0003) { "not an AXML document (type=0x%04x)".format(headerType) }
        buf.short // headerSize
        if (buf.remaining() < 4) throw IllegalArgumentException("truncated header")
        buf.int // size

        var pool: StringPool? = null
        var packageName: String? = null
        var versionName: String? = null
        var versionCode: Long? = null

        while (buf.remaining() >= 8) {
            val chunkStart = buf.position()
            val chunkType = buf.short.toInt() and 0xffff
            buf.short // headerSize
            val chunkSize = buf.int
            // Guard against malformed sizes.
            if (chunkSize < 8 || chunkSize > buf.limit() - chunkStart) break
            val payloadEnd = chunkStart + chunkSize

            when (chunkType) {
                CHUNK_STRING_POOL -> {
                    pool = parseStringPool(buf, payloadEnd)
                }
                CHUNK_START_ELEMENT -> {
                    // Skip the fixed part of the element header.
                    buf.int // lineNumber
                    buf.int // comment
                    buf.int // ns
                    val nameIdx = buf.int
                    val attributeStart = buf.short.toInt() and 0xffff
                    val attributeSize = buf.short.toInt() and 0xffff
                    val attributeCount = buf.short.toInt() and 0xffff
                    buf.short // idIndex
                    buf.short // classIndex

                    val elementName = pool?.get(nameIdx)

                    if (elementName == "manifest") {
                        // attributeStart is relative to the start of the
                        // *named* element extension, which begins at byte 16
                        // after the chunk header (node header: lineNumber +
                        // comment). Attributes therefore begin at
                        // chunkStart + 16 + attributeStart.
                        val attrsStart = chunkStart + 16 + attributeStart
                        val recordSize = if (attributeSize >= 20) attributeSize else 20

                        for (i in 0 until attributeCount) {
                            val pos = attrsStart + i * recordSize
                            if (pos + 20 > payloadEnd) break
                            val attr = readAttribute(pool, buf, pos)
                            when (attr.name) {
                                "versionCode" -> versionCode =
                                    intValue(attr.dataType, attr.data)
                                "versionName" -> {
                                    if (attr.dataType == TYPE_STRING) {
                                        versionName = attr.stringValue
                                    } else if (versionName == null) {
                                        versionName = attr.stringValue
                                    }
                                }
                                "package" -> packageName = attr.stringValue
                            }
                        }
                    }
                    // Skip to the end of this chunk regardless.
                    buf.position(payloadEnd)
                }
                else -> buf.position(payloadEnd)
            }
            buf.position(payloadEnd)
        }

        return ApkMeta(
            packageName = packageName ?: "com.mojang.minecraftpe",
            versionName = versionName ?: "unknown",
            versionCode = versionCode ?: 0,
        )
    }

    /** Read a single attribute record at absolute position `pos`. */
    private fun readAttribute(
        pool: StringPool?,
        buf: ByteBuffer,
        pos: Int,
    ): Attribute {
        val saved = buf.position()
        buf.position(pos)
        val aNs = buf.int
        val aName = buf.int
        val rawValue = buf.int
        val typedSize = buf.short.toInt() and 0xffff
        buf.get() // res0
        val dataType = buf.get().toInt() and 0xff
        val data = buf.int
        buf.position(saved)

        val name = pool?.get(aName)
        val stringValue = when {
            dataType == TYPE_STRING -> pool?.get(data)
            rawValue >= 0 -> pool?.get(rawValue)
            else -> null
        }
        return Attribute(
            name = name ?: "",
            dataType = dataType,
            data = data,
            rawValue = rawValue,
            stringValue = stringValue,
        )
    }

    private fun intValue(dataType: Int, data: Int): Long {
        if (dataType == TYPE_INT_DEC || dataType == TYPE_INT_HEX) return data.toLong() and 0xffff_ffffL
        return data.toLong()
    }

    private class StringPool(private val strings: List<String>) {
        fun get(index: Int): String? =
            if (index in 0 until strings.size) strings[index] else null
    }

    private data class Attribute(
        val name: String,
        val dataType: Int,
        val data: Int,
        val rawValue: Int,
        val stringValue: String?,
    )

    /**
     * Parse a string pool chunk. Positioned at the stringCount field; ends at
     * `payloadEnd`.
     */
    private fun parseStringPool(buf: ByteBuffer, payloadEnd: Int): StringPool {
        // Pool header (fields after the chunk header triple).
        val stringCount = buf.int
        val styleCount = buf.int
        val flags = buf.int
        val stringsStart = buf.int
        val stylesStart = buf.int
        val utf8 = (flags and UTF8_FLAG) != 0

        val stringOffsets = IntArray(stringCount) { buf.int }
        IntArray(styleCount) { buf.int } // styleOffsets

        // Recover absolute positions.
        val posNow = buf.position()
        // The string offsets are relative to the chunk start. `posNow` is
        // `chunkStart + 8 (chunk header) + 20 (pool fields) +
        // stringCount*4 + styleCount*4`. Back out the chunk start.
        val consumed = 8 + 20 + stringCount * 4 + styleCount * 4
        val chunkStart = posNow - consumed
        val blobStart = chunkStart + stringsStart

        val strings = ArrayList<String>(stringCount)
        val savedPos = buf.position()
        for (i in 0 until stringCount) {
            val abs = blobStart + stringOffsets[i]
            if (abs < 0 || abs >= payloadEnd) {
                strings += ""
                continue
            }
            buf.position(abs)
            strings += if (utf8) readUtf8(buf, payloadEnd) else readUtf16(buf, payloadEnd)
        }
        buf.position(savedPos)
        buf.position(payloadEnd)
        return StringPool(strings)
    }

    private fun readUtf8(buf: ByteBuffer, limit: Int): String {
        if (buf.position() >= limit) return ""
        val charLen = readLen(buf)
        val byteLen = readLen(buf)
        if (buf.position() + byteLen > limit) return ""
        val bytes = ByteArray(byteLen)
        buf.get(bytes)
        return try {
            String(bytes, StandardCharsets.UTF_8).take(charLen)
        } catch (_: Exception) {
            ""
        }
    }

    private fun readUtf16(buf: ByteBuffer, limit: Int): String {
        // UTF-16 entries encode the character count as a 2-byte length.
        if (buf.position() + 2 > limit) return ""
        val charLen = buf.short.toInt() and 0xffff
        val nBytes = charLen * 2
        if (buf.position() + nBytes > limit) return ""
        val bytes = ByteArray(nBytes)
        buf.get(bytes)
        return try {
            String(bytes, StandardCharsets.UTF_16LE)
        } catch (_: Exception) {
            ""
        }
    }

    private fun readLen(buf: ByteBuffer): Int {
        val first = buf.get().toInt() and 0xff
        return if (first and 0x80 != 0) {
            val second = buf.get().toInt() and 0xff
            ((first and 0x7f) shl 8) or second
        } else {
            first
        }
    }
}