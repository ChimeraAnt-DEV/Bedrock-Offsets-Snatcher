cmake_minimum_required(VERSION 3.22.1)
project(glowberry_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(glowberry_tests
    ../app/src/main/cpp/elf_parser.cpp
    ../app/src/main/cpp/scanner.cpp
    ../app/src/main/cpp/signatures.cpp
    ../app/src/main/cpp/extractor.cpp
    ../app/src/main/cpp/json_gen.cpp
    test_main.cpp
    test_elf_parser.cpp
    test_scanner.cpp
    test_extractor.cpp
    test_json.cpp
)
target_include_directories(glowberry_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp"
)
target_compile_options(glowberry_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME glowberry_tests COMMAND glowberry_tests)