package de.lynxifyy.offsetsnatcher.processing

import org.json.JSONArray
import org.json.JSONObject

/** Minecraft package meta extracted from the APK. */
data class ApkMeta(
    val packageName: String,
    val versionName: String,
    val versionCode: Long,
)

/** A single resolved offset published in the JSON output. */
data class FunctionEntry(
    val name: String,
    val category: String,
    val offset: Long,
    val verified: Boolean,
    val adjust: Long = 0,
)

/** A discovered vtable region. */
data class VtableEntry(
    val name: String,
    val offset: Long,
    val size: Long,
    val methodCount: Int,
)

/** The full extraction result returned from native code. */
data class ExtractResult(
    val ok: Boolean,
    val error: String? = null,
    val minecraftVersion: String = "",
    val abi: String = "",
    val baseAddress: Long = 0,
    val functions: List<FunctionEntry> = emptyList(),
    val vtables: List<VtableEntry> = emptyList(),
) {
    /** Render the result as a pretty-printed JSON document (BerryAPI schema). */
    fun toJson(): String {
        val root = JSONObject()
        root.put("ok", ok)
        if (error != null) root.put("error", error)
        root.put("minecraft_version", minecraftVersion)
        root.put("abi", abi)
        root.put("base_address", baseAddress)

        val funcs = JSONArray()
        for (f in functions) {
            val o = JSONObject()
            o.put("name", f.name)
            o.put("category", f.category)
            o.put("offset", f.offset)
            o.put("verified", f.verified)
            o.put("adjust", f.adjust)
            funcs.put(o)
        }
        root.put("functions", funcs)

        val vts = JSONArray()
        for (v in vtables) {
            val o = JSONObject()
            o.put("name", v.name)
            o.put("offset", v.offset)
            o.put("size", v.size)
            o.put("method_count", v.methodCount)
            vts.put(o)
        }
        root.put("vtables", vts)
        return root.toString(2)
    }

    companion object {
        /** Parse the JSON document returned by Bridge.extract(). */
        fun fromJson(json: String): ExtractResult {
            val root = JSONObject(json)
            val ok = root.optBoolean("ok", false)
            val error = if (root.isNull("error")) null else root.optString("error", null)
            val funcs = mutableListOf<FunctionEntry>()
            val funcArr = root.optJSONArray("functions") ?: JSONArray()
            for (i in 0 until funcArr.length()) {
                val o = funcArr.getJSONObject(i)
                funcs += FunctionEntry(
                    name = o.optString("name"),
                    category = o.optString("category"),
                    offset = o.optLong("offset"),
                    verified = o.optBoolean("verified", true),
                    adjust = o.optLong("adjust", 0),
                )
            }
            val vts = mutableListOf<VtableEntry>()
            val vtArr = root.optJSONArray("vtables") ?: JSONArray()
            for (i in 0 until vtArr.length()) {
                val o = vtArr.getJSONObject(i)
                vts += VtableEntry(
                    name = o.optString("name"),
                    offset = o.optLong("offset"),
                    size = o.optLong("size"),
                    methodCount = o.optInt("method_count"),
                )
            }
            return ExtractResult(
                ok = ok,
                error = error,
                minecraftVersion = root.optString("minecraft_version"),
                abi = root.optString("abi"),
                baseAddress = root.optLong("base_address"),
                functions = funcs,
                vtables = vts,
            )
        }
    }
}