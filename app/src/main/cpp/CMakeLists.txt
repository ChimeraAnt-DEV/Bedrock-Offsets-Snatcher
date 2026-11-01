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
# Android toolchain).
add_executable(glowberry_tests
    elf_parser.cpp
    scanner.cpp
    signatures.cpp
    extractor.cpp
    tests/test_elf_parser.cpp
    tests/test_scanner.cpp
    tests/test_extractor.cpp
)
target_include_directories(glowberry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(glowberry_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME glowberry_tests COMMAND glowberry_tests)