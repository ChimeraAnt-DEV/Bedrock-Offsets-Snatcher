// Minimal test framework (no external deps) for the extraction engine.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace glowberry_test {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

#define TEST(name_)                                                  \
    static void name_##_impl();                                      \
    static ::glowberry_test::Registrar name_##_reg(#name_,           \
        name_##_impl);                                               \
    static void name_##_impl()

#define EXPECT_TRUE(cond)                                              \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,          \
                         __LINE__, #cond);                              \
            ::glowberry_test::failures()++;                            \
        }                                                              \
    } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

template <typename A, typename B>
bool expectEqImpl(const char* file, int line, const char* ea, const char* eb,
                  const A& a, const B& b) {
    if (a == b) return true;
    std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", file, line, ea, eb);
    ::glowberry_test::failures()++;
    return false;
}

#define EXPECT_EQ(a, b)                                                        \
    do {                                                                       \
        ::glowberry_test::expectEqImpl(__FILE__, __LINE__, #a, #b, (a), (b));  \
    } while (0)

inline int runAll(const char*) {
    int ran = 0;
    for (const TestCase& t : registry()) {
        std::fprintf(stderr, "[test] %s\n", t.name);
        t.fn();
        ++ran;
    }
    if (failures() == 0) {
        std::fprintf(stderr, "OK: all %d test(s) passed\n", ran);
        return 0;
    }
    std::fprintf(stderr, "FAILED: %d failure(s) across %d test(s)\n",
                 failures(), ran);
    return 1;
}

}  // namespace glowberry_test