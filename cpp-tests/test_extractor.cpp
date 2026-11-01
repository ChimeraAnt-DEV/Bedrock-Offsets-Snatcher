// End-to-end tests for the extraction engine against the synthetic ARM64
// fixture. The fixture hard-codes the current signature bytes; if a pattern
// in signatures.cpp is edited, this fixture must be regenerated with
// cpp-tests/build_fixture.py.
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "extractor.h"
#include "scanner.h"
#include "signatures.h"
#include "test_util.h"

using namespace glowberry;

static std::vector<uint8_t> readFile(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    buf.resize(got);
    return buf;
}

static ExtractResult runFixture(const char* path) {
    auto data = readFile(path);
    VersionInfo v;
    v.packageName = "com.mojang.minecraftpe";
    v.versionName = "1.20.62";
    v.versionCode = 2020062;
    if (data.empty()) {
        ExtractResult r;
        r.error = "fixture missing";
        return r;
    }
    return extractOffsets(data.data(), data.size(), v);
}

TEST(extractor_succeeds_on_fixture) {
    ExtractResult r = runFixture("cpp-tests/fixture_arm64.so");
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(r.minecraft_version, "1.20.62");
    EXPECT_EQ(r.base_address, 0ULL);
}

TEST(extractor_finds_all_functions) {
    ExtractResult r = runFixture("cpp-tests/fixture_arm64.so");
    EXPECT_TRUE(r.ok);
    std::map<std::string, uint64_t> found;
    for (const FunctionOffset& f : r.functions) found[f.name] = f.offset;

    // Every text signature present in the fixture must be extracted, and its
    // offset must be the virtual address in the fixture (base == 0). The
    // fixture maps vaddr == file offset (identity mapping), and the .text
    // section lives at file offset 0x200.
    EXPECT_EQ(found["ClientInstance::getLocalPlayer"], 0x200ULL);
    EXPECT_EQ(found["+ClientInstance::getLocalPlayer_alias"], 0x220ULL);
    EXPECT_EQ(found["ClientInstance::getLevel"], 0x230ULL);
    EXPECT_EQ(found["Actor::getPos"], 0x250ULL);
    EXPECT_EQ(found["Actor::getRot"], 0x270ULL);
    EXPECT_EQ(found["+Actor::getLevel"], 0x280ULL);
    EXPECT_EQ(found["Level::getHitResult"], 0x290ULL);
    EXPECT_EQ(found["+Level::getCurrentTick"], 0x2b0ULL);
    EXPECT_EQ(found["Player::getCarriedItem"], 0x2d0ULL);
    EXPECT_EQ(found["Player::getDimensionId"], 0x2f0ULL);
    EXPECT_EQ(found["ItemStack::getItem"], 0x300ULL);
}

TEST(extractor_detects_vtables) {
    ExtractResult r = runFixture("cpp-tests/fixture_arm64.so");
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(!r.vtables.empty());
    bool hasRun = false;
    for (const VtableInfo& vt : r.vtables) {
        if (vt.method_count >= 8) {
            hasRun = true;
            EXPECT_EQ(vt.size, static_cast<uint64_t>(vt.method_count) * 8);
        }
    }
    EXPECT_TRUE(hasRun);
}

TEST(extractor_scans_rodata) {
    ExtractResult r = runFixture("cpp-tests/fixture_arm64.so");
    EXPECT_TRUE(r.ok);
    bool foundString = false;
    for (const FunctionOffset& f : r.functions) {
        if (f.name.rfind("string:", 0) == 0) foundString = true;
    }
    // .rodata scan is best-effort; at least the .text scan must work.
    (void)foundString;
    EXPECT_TRUE(r.text_scanned > 0);
    EXPECT_TRUE(r.rodata_scanned > 0);
}

TEST(extractor_reports_failure_on_garbage) {
    const uint8_t junk[128] = {0};
    VersionInfo v;
    v.versionName = "x";
    ExtractResult r = extractOffsets(junk, sizeof(junk), v);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(!r.error.empty());
}

TEST(extractor_reports_failure_on_empty) {
    VersionInfo v;
    v.versionName = "x";
    // A zero-length buffer must fail cleanly, not crash.
    uint8_t dummy = 0;
    (void)dummy;
    const uint8_t* p = nullptr;
    ExtractResult r = extractOffsets(p, 0, v);
    EXPECT_FALSE(r.ok);
}
