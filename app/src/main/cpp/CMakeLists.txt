cmake_minimum_required(VERSION 3.22.1)

project("glowberry_extractor" CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------- library ---
add_library(glowberry_extractor SHARED
    elf_parser.cpp
    scanner.cpp
    signatures.cpp
    extractor.cpp
    jni_bridge.cpp
)

# Use the NDK's bundled LLVM libc++ (shared variant keeps the APK small when
# multiple consumers link it, and is the default on arm64-v8a).
target_compile_options(glowberry_extractor PRIVATE
    -Wall -Wextra
)
target_link_libraries(glowberry_extractor PRIVATE log android)

# ------------------------------------------------------------- host tests ---
# The extraction engine is architecture independent; compile the same sources
# into a native test binary (built for the host when running without the
# Android toolchain). The tests live in cpp-tests/ at the repo root and are
# driven by cpp-tests/CMakeLists.txt, which is intentionally NOT part of the
# Android build (no AndroidManifest/JNI needed).