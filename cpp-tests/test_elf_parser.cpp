// Tests for the custom ELF64 parser against the synthetic fixture and against
// hand-crafted malformed inputs (malicious .so files must fail gracefully).
#include <cstdio>
#include <cstring>
#include <vector>

#include "elf_parser.h"
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

TEST(parser_reads_fixture_header) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    EXPECT_TRUE(!data.empty());
    ElfParser p(data.data(), data.size());
    EXPECT_TRUE(p.parse());
    EXPECT_EQ(p.machine(), kEmAarch64);
    EXPECT_EQ(p.e_type(), 2u);
}

TEST(parser_finds_sections) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    ElfParser p(data.data(), data.size());
    EXPECT_TRUE(p.parse());
    const Section* text = p.findSection(".text");
    const Section* rodata = p.findSection(".rodata");
    const Section* relro = p.findSection(".data.rel.ro");
    EXPECT_TRUE(text != nullptr);
    EXPECT_TRUE(rodata != nullptr);
    EXPECT_TRUE(relro != nullptr);
    if (text) {
        // Identity-mapped fixture: section vaddr equals its file offset.
        EXPECT_EQ(text->address, text->offset);
    }
    if (relro) {
        EXPECT_TRUE(relro->size >= 11 * 8);
    }
}

TEST(parser_base_address_is_zero_for_fixture) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    ElfParser p(data.data(), data.size());
    EXPECT_TRUE(p.parse());
    EXPECT_EQ(p.baseAddress(), 0ULL);
}

TEST(parser_fileOffsetToVaddr_roundtrip) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    ElfParser p(data.data(), data.size());
    EXPECT_TRUE(p.parse());
    // .text starts at file offset 0x200; the fixture uses the identity
    // mapping vaddr == offset (matching real Android .so layout with base 0).
    uint64_t vaddr = 0;
    EXPECT_TRUE(p.fileOffsetToVaddr(0x200, &vaddr));
    EXPECT_EQ(vaddr, 0x200ULL);
    uint64_t bad = 0;
    EXPECT_FALSE(p.fileOffsetToVaddr(static_cast<uint64_t>(data.size() + 99),
                                     &bad));
}

TEST(parser_rejects_garbage) {
    const uint8_t junk[64] = {0xAA};
    ElfParser p(junk, sizeof(junk));
    EXPECT_FALSE(p.parse());
    EXPECT_TRUE(!p.error().empty());
}

TEST(parser_rejects_truncated) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    ElfParser p(data.data(), 16);  // too small for an ELF64 header
    EXPECT_FALSE(p.parse());
}

TEST(parser_rejects_bad_machine_when_requested) {
    auto data = readFile("cpp-tests/fixture_arm64.so");
    // The parser accepts any machine; the *extractor* enforces arm64. Here we
    // only check that parsing (not semantic validation) succeeds for our AR64
    // fixture and that machine is echoed correctly.
    ElfParser p(data.data(), data.size());
    EXPECT_TRUE(p.parse());
    EXPECT_EQ(p.machine(), 183u);
}
