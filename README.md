# GlowBerry Offset Snatcher

An Android app (Kotlin + Jetpack Compose + C++/NDK) that takes a Minecraft
Bedrock Edition **arm64-v8a** APK, extracts `libminecraftpe.so`, reverse-
engineers it entirely **on-device**, and emits the offsets + signature hits
needed for BerryAPI as a JSON document.

![theme](app/src/main/res/drawable/ic_launcher_foreground.png)

## Pipeline

```
APK  ──▶  extract lib/arm64-v8a/libminecraftpe.so  ──▶  parse ELF64
   │                                                     (program headers, sections, .dynsym)
   │
   └──▶  AndroidManifest.xml (AXML)  ──▶  Minecraft version
                                             │
          pattern scan (.text / .rodata)  ◀──┘   vtable scan (.data.rel.ro)
                                             │
                                             ▼
                              BerryAPI JSON (copy / export)
```

| Phase | Component | Notes |
|---|---|---|
| 1 | `ApkExtractor.kt` | Unzips on `Dispatchers.IO`, private storage only |
| 1 | `ManifestParser.kt` | Binary-XML parser for versionCode/versionName/package |
| 2 | `elf_parser.cpp` | Custom bounds-checked ELF64 parser (arm64) |
| 2 | `jni_bridge.cpp` | `Bridge.extract()` → JSON |
| 3 | `scanner.cpp` | memchr-anchored wildcard (`??`) pattern scanner |
| 3 | `signatures.cpp` | Curated IDA-style signature DB (editable) |
| 3 | `extractor.cpp` | Offset + `.data.rel.ro` vtable pointer-run extraction |
| 4 | `MainScreen.kt` | JSON preview, Copy, Export `.json` |
| 5 | `MainViewModel.kt` | Background pipeline, animated progress |

## Privacy & security

- **100% on-device**: no network permission is declared. The APK and all
  extracted offsets never leave the handset.
- Extracted library is stored with owner-only permissions in
  `filesDir/glowberry/`.
- The C++ parser is strictly bounds-checked so a malformed `.so` fails
  cleanly instead of crashing.

## Signature database

`app/src/main/cpp/signatures.cpp` holds the pattern list. To add/refresh
signatures for new game versions, edit that file and regenerate the test
fixture:

```bash
python3 cpp-tests/build_fixture.py
```

The fixture is compiled into the host tests, which means the patterns and the
tests can never drift apart.

## Building

Prereqs: JDK 17+, Android SDK (platform 34, build-tools 34.0.0), NDK 27,
CMake 3.22.1.

```bash
export ANDROID_HOME=/path/to/sdk
gradle :app:assembleDebug
gradle :app:testDebugUnitTest
# or, with the wrapper:
./gradlew :app:assembleDebug
```

Native host tests (no Android needed):

```bash
cmake -S cpp-tests -B /tmp/tests && cmake --build /tmp/tests && /tmp/tests/glowberry_tests
```

## License

This project is for educational and interoperability purposes. Minecraft and
Minecraft Bedrock Edition are trademarks of Mojang/Microsoft.