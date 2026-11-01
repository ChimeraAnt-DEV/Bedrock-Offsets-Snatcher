// ELF64 (little-endian) parser used by the GlowBerry offset extractor.
// Only what is needed for static analysis of ARM64 .so files is read:
// header, program headers, section headers and the dynamic symbol table.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace glowberry {

constexpr uint16_t kEmAarch64 = 183;
constexpr uint32_t kShnUndef = 0;
constexpr uint32_t kPtLoad = 1;

struct Section {
    std::string name;
    uint32_t type = 0;
    uint64_t flags = 0;
    uint64_t address = 0;   // sh_addr
    uint64_t offset = 0;    // sh_offset
    uint64_t size = 0;      // sh_size
    uint32_t link = 0;
    uint32_t info = 0;
    uint64_t addralign = 0;
    uint64_t entsize = 0;
};

struct ProgramHeader {
    uint32_t type = 0;
    uint64_t flags = 0;
    uint64_t offset = 0;
    uint64_t vaddr = 0;
    uint64_t filesz = 0;
    uint64_t memsz = 0;
    uint64_t align = 0;
};

struct Symbol {
    std::string name;
    uint64_t value = 0;   // st_value (virtual address)
    uint64_t size = 0;
    uint8_t info = 0;     // ST_BIND / ST_TYPE
    uint16_t shndx = 0;
    bool defined = false; // st_shndx != SHN_UNDEF
};

class ElfParser {
public:
    // `data` must stay alive while this object is used.
    ElfParser(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool parse();
    const std::string& error() const { return error_; }

    bool valid() const { return parsed_; }
    uint16_t machine() const { return machine_; }
    uint16_t e_type() const { return e_type_; }

    // The in-memory base of the image computed from PT_LOAD segments:
    // base = min over PT_LOAD of (p_vaddr - p_offset). For "normal" Android
    // .so files the first segment maps vaddr 0 -> offset 0, so base == 0 and
    // every symbol/pattern offset is already relative to that base.
    uint64_t baseAddress() const { return base_; }

    // Map a file offset to the virtual address it is loaded at.
    // Returns false if the offset is not covered by any PT_LOAD segment.
    bool fileOffsetToVaddr(uint64_t file_offset, uint64_t* vaddr) const;

    const std::vector<Section>& sections() const { return sections_; }
    const std::vector<ProgramHeader>& programs() const { return programs_; }
    const std::vector<Symbol>& symbols() const { return symbols_; }

    const Section* findSection(const std::string& name) const;
    // Section payload slice, clamped to the file bounds.
    bool sectionData(const Section& s, const uint8_t** ptr, size_t* len) const;

private:
    bool readLsb32(size_t pos, uint32_t* out) const;
    bool readLsb64(size_t pos, uint64_t* out) const;

    const uint8_t* data_;
    size_t size_;
    std::string error_;
    bool parsed_ = false;
    uint16_t e_type_ = 0;
    uint16_t machine_ = 0;
    uint64_t base_ = 0;
    std::vector<Section> sections_;
    std::vector<ProgramHeader> programs_;
    std::vector<Symbol> symbols_;
    std::vector<char> string_table_;   // concatenated section names
    std::vector<Symbol> dynsym_raw_;
};

}  // namespace glowberry
