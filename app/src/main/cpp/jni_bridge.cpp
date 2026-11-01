// JNI bridge: Kotlin (APK processing) <-> native (ELF parsing & scanning).
//
// Exposed entry point:
//   com.globberry.Bridge / de.lynxifyy.offsetsnatcher.Bridge
//   String extract(String soPath, String packageName, String versionName,
//                  int versionCode)
//
// Returns a JSON document of the form:
//   {
//     "ok": true|false,
//     "error": "...",              // present when ok == false
//     "minecraft_version": "1.20.62",
//     "abi": "arm64-v8a",
//     "base_address": 0,
//     "functions": [ { "name": "...", "category": "text|rodata",
//                      "offset": 1234, "verified": true, "adjust": 0 } ],
//     "vtables": [ { "name": "vtable@0x...", "offset": 0, "size": 512,
//                    "method_count": 64 } ]
//   }
//
// The whole pipeline is local to the device; no network access happens here.
#include <jni.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "elf_parser.h"
#include "extractor.h"
#include "json_gen.h"
#include "scanner.h"

namespace glowberry {

}  // namespace glowberry

using namespace glowberry;

extern "C" JNIEXPORT jstring JNICALL
Java_de_lynxifyy_offsetsnatcher_Bridge_extract(JNIEnv* env, jclass,
                                               jstring soPath,
                                               jstring packageName,
                                               jstring versionName,
                                               jint versionCode) {
    const char* path = env->GetStringUTFChars(soPath, nullptr);
    if (!path) return env->NewStringUTF(R"({"ok":false,"error":"bad input"})");

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::string res = "{\"ok\":false,\"error\":\"cannot open ";
        res += path;
        res += "\"}";
        env->ReleaseStringUTFChars(soPath, path);
        return env->NewStringUTF(res.c_str());
    }

    std::vector<uint8_t> image;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);
    if (sz > 0) {
        image.resize(static_cast<size_t>(sz));
        if (std::fread(image.data(), 1, image.size(), f) != image.size()) {
            std::fclose(f);
            env->ReleaseStringUTFChars(soPath, path);
            return env->NewStringUTF(
                R"({"ok":false,"error":"short read loading library"})");
        }
    }
    std::fclose(f);

    VersionInfo version{};
    version.packageName = "com.mojang.minecraftpe";
    if (packageName) {
        const char* cs = env->GetStringUTFChars(packageName, nullptr);
        if (cs) {
            version.packageName = cs;
            env->ReleaseStringUTFChars(packageName, cs);
        }
    }
    if (versionName) {
        const char* cs = env->GetStringUTFChars(versionName, nullptr);
        if (cs) {
            version.versionName = cs;
            env->ReleaseStringUTFChars(versionName, cs);
        }
    }
    version.versionCode = versionCode;

    ExtractResult res;
    if (image.empty()) {
        res.error = "empty library file";
    } else {
        res = extractOffsets(image.data(), image.size(), version);
    }
    env->ReleaseStringUTFChars(soPath, path);

    std::string json = buildJson(res);
    return env->NewStringUTF(json.c_str());
}