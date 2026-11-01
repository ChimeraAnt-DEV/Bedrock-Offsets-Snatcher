# AGENTS.md — Bedrock-Offsets-Snatcher

Project rules and conventions for AI agents working in this repository.
Follow these strictly across all sessions.

## Repository

- **What this is**: GlowBerry Offset Snatcher — an Android app (Kotlin +
  Jetpack Compose + C++/NDK) that accepts a Minecraft Bedrock Edition
  arm64-v8a `.apk`, reverse-engineers `libminecraftpe.so` fully **on-device**
  and exports the offsets BerryAPI needs as JSON.
- **Stack**: Kotlin, Jetpack Compose, Material3; C++17 via NDK (CMake
  3.22.1, NDK 27.2.12479018); Gradle 8.7 wrapper; AGP 8.4.2; Kotlin 1.9.24;
  compileSdk 34 / minSdk 26 / targetSdk 34.
- **Architecture**:
  - `app/src/main/java/de/lynxifyy/offsetsnatcher/` — Kotlin (UI, ViewModel,
    APK extraction, AXML manifest parsing, native bridge).
  - `app/src/main/cpp/` — native engine: `elf_parser`, `scanner`,
    `signatures`, `extractor`, `json_gen`, `jni_bridge`.
  - `cpp-tests/` — host-side C++ tests + `build_fixture.py` generator
    (regenerates `fixture_arm64.so` straight from `signatures.cpp`).

## Workflow rules (mandatory)

1. **Test first.** Write/update tests before or together with code. Never
   ship changes that break the suite: `cmake --build` + run
   `glowberry_tests` (host) and `gradle :app:testDebugUnitTest` +
   `gradle :app:assembleDebug` (Android).
2. **Edit with Python for precision** — write/modify files via python scripts
   (e.g. heredocs) rather than interactive editor calls; then verify file
   contents on disk after writing.
3. **No sloppy/beginner mistakes.** No typos, no half-implemented shells.
   A structure must contain its real implementation, state and behaviour —
   not just an outer container that "looks done".
4. **Complete everything.** Do not leave tasks unfinished or skipped; keep
   iterating until tests pass and the feature is genuinely complete.
5. **No unnecessary constraints** — if a task conflicts with an assumption,
   the explicit instruction wins.

## Git rules (mandatory)

- **All commits must be authored by `Lynxifyy` only.** Never use
  `openhands` and never add a `Co-authored-by: openhands` trailer.
  ```
  git -c user.name=Lynxifyy \
      -c user.email=Lynxifyy@users.noreply.github.com \
      commit -m "..."
  ```
- **Phase-based commits.** Each phase gets its own commit(s) describing what
  that phase changes.
- When a PR is requested: push a feature branch and open exactly **one** PR
  for the session/issue.
- Do not push directly to `main` unless explicitly asked.

## Native test commands (host, no Android needed)

```bash
python3 cpp-tests/build_fixture.py          # regenerate fixture
cmake -S cpp-tests -B /tmp/tests && cmake --build /tmp/tests -j"$(nproc)"
/tmp/tests/glowberry_tests                   # expect: OK: all N passed
```

## Android build commands

```bash
export ANDROID_HOME=/path/to/sdk
gradle :app:assembleDebug
gradle :app:testDebugUnitTest
```

## CI

`.github/workflows/build.yml` builds + tests on push/PR to `main` and
publishes a GitHub release (with the APK attached) on `v*` tags.

## Security

- **Privacy is a feature**: no network permission, no analytics, nothing may
  ever be uploaded. All APK/ELF processing is local.
- Native code is bounds-checked; malformed `.so` files must fail cleanly.
