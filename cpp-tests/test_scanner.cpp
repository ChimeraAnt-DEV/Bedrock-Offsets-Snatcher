// Tests for the wildcard-aware pattern scanner.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "scanner.h"
#include "test_util.h"

using namespace glowberry;

TEST(scanner_finds_exact_match) {
    const uint8_t hay[] = {0xAA, 0xBB, 0x01, 0x02, 0x03, 0xCC};
    size_t pos = 123;
    size_t plen = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "01 02 03",
                                            &pos, &plen));
    EXPECT_EQ(pos, 2u);
    EXPECT_EQ(plen, 3u);
}

TEST(scanner_wildcards_middle) {
    const uint8_t hay[] = {0xAA, 0xBB, 0x01, 0x99, 0x03, 0xCC};
    size_t pos = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "01 ?? 03",
                                            &pos, nullptr));
    EXPECT_EQ(pos, 2u);
}

TEST(scanner_wildcards_multi_byte) {
    const uint8_t hay[] = {0xAA, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    size_t pos = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "11 ?? 33 44 ??",
                                            &pos, nullptr));
    EXPECT_EQ(pos, 1u);
}

TEST(scanner_leading_wildcards) {
    const uint8_t hay[] = {0x00, 0x01, 0xAB, 0x02};
    size_t pos = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "?? AB ??",
                                            &pos, nullptr));
    EXPECT_EQ(pos, 1u);
}

TEST(scanner_not_found) {
    const uint8_t hay[] = {0x01, 0x02, 0x03, 0x04};
    size_t pos = 0;
    EXPECT_FALSE(SignatureScanner::findFirst(hay, sizeof(hay), "FF EE",
                                             &pos, nullptr));
}

TEST(scanner_pattern_longer_than_haystack) {
    const uint8_t hay[] = {0x01};
    EXPECT_FALSE(SignatureScanner::findFirst(hay, sizeof(hay), "01 02",
                                             nullptr, nullptr));
}

TEST(scanner_empty_pattern) {
    const uint8_t hay[] = {0x01};
    EXPECT_FALSE(SignatureScanner::findFirst(hay, sizeof(hay), "", nullptr,
                                             nullptr));
}

TEST(scanner_dot_as_wildcard) {
    const uint8_t hay[] = {0xAA, 0x01, 0x77, 0x03};
    size_t pos = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "01 . 03",
                                            &pos, nullptr));
    EXPECT_EQ(pos, 1u);
}

TEST(scanner_case_insensitive_hex) {
    const uint8_t hay[] = {0xBe, 0xEf};
    size_t pos = 0;
    EXPECT_TRUE(SignatureScanner::findFirst(hay, sizeof(hay), "BE EF",
                                            &pos, nullptr));
    EXPECT_EQ(pos, 0u);
}

// Fuzz: for random haystacks over a handful of patterns, the scanner
// (jump-based) must agree with a brute-force reference.
static bool bruteForce(const uint8_t* hay, size_t n, const std::string& pat,
                       size_t* pos) {
    std::vector<uint8_t> bytes;
    std::vector<bool> wc;
    compilePattern(pat, &bytes, &wc);
    for (size_t i = 0; i + bytes.size() <= n; ++i) {
        bool ok = true;
        for (size_t j = 0; j < bytes.size(); ++j) {
            if (!wc[j] && hay[i + j] != bytes[j]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            *pos = i;
            return true;
        }
    }
    return false;
}

TEST(scanner_fuzz_agrees_with_bruteforce) {
    const char* patterns[] = {
        "FD 7B BE A9 E0 03 00 F9",
        "08 ?? 40 F9 00 00 40 FD",
        "?? ?? 94 ?? ?? ?? F9",
        "4D 69 6E 65 63 72 61 66 74",
        "?? ?? ?? ??",
        "AA",
    };
    uint16_t state = 0xC0FF;
    auto rnd = [&state]() -> uint8_t {
        state = static_cast<uint16_t>(state * 131 + 17);
        return static_cast<uint8_t>(state >> 8);
    };
    for (int iter = 0; iter < 400; ++iter) {
        size_t n = 1 + (rnd() % 48);
        std::vector<uint8_t> hay(n);
        for (size_t i = 0; i < n; ++i) hay[i] = (iter % 3 == 0) ? rnd()
                                                      : rnd() & 0x7f;
        const char* pat = patterns[rnd() % (sizeof(patterns) /
                                            sizeof(patterns[0]))];
        size_t a = 0, b = 0;
        bool ra = SignatureScanner::findFirst(hay.data(), hay.size(), pat, &a,
                                              nullptr);
        bool rb = bruteForce(hay.data(), hay.size(), pat, &b);
        if (ra != rb || (ra && a != b)) {
            std::fprintf(stderr,
                         "MISMATCH iter=%d pat=%s n=%zu fast=(%d,%zu) "
                         "brute=(%d,%zu)\n",
                         iter, pat, n, ra, a, rb, b);
            EXPECT_TRUE(false);
            return;
        }
    }
    EXPECT_TRUE(true);
}
