#include "elf_parser.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace glowberry {
namespace {

// ELF64 e_ident offsets.
constexpr size_t kEiClass = 4;  // 2 = ELFCLASS64
constexpr size_t kEiData = 5;   // 1 = little-endian

// ELF64 header field offsets (64-byte header).
constexpr size_t kEhType = 16;
constexpr size_t kEhMachine = 18;
constexpr size_t kEhPhoff = 32;
constexpr size_t kEhShoff = 40;
constexpr size_t kEhPhentsize = 54;
constexpr size_t kEhPhnum = 56;
constexpr size_t kEhShentsize = 58;
constexpr size_t kEhShnum = 60;
constexpr size_t kEhShstrndx = 62;

// ELF64 program header field offsets (56-byte entries).
constexpr size_t kPhType = 0;
constexpr size_t kPhFlags = 4;
constexpr size_t kPhOffset = 8;
constexpr size_t kPhVaddr = 16;
constexpr size_t kPhFilesz = 32;
constexpr size_t kPhMemsz = 40;
constexpr size_t kPhAlign = 48;

// ELF64 section header field offsets (64-byte entries).
constexpr size_t kShName = 0;
constexpr size_t kShType = 4;
constexpr size_t kShFlags = 8;
constexpr size_t kShAddr = 16;
constexpr size_t kShOffset = 24;
constexpr size_t kShSize = 32;
constexpr size_t kShLink = 40;
constexpr size_t kShInfo = 44;
constexpr size_t kShAddralign = 48;
constexpr size_t kShEntsize = 56;

// ELF64 dynamic symbol entry field offsets (24-byte entries).
constexpr size_t kSymName = 0;
constexpr size_t kSymInfo = 4;
constexpr size_t kSymShndx = 6;
constexpr size_t kSymValue = 8;
constexpr size_t kSymSize = 16;

constexpr uint32_t kShtDynsym = 11;


inline uint16_t load16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
inline uint32_t load32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t load64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

std::string readCString(const uint8_t* data, size_t len, size_t pos,
                        size_t max_len = 512) {
    if (pos >= len) return {};
    size_t limit = std::min(len, pos + max_len);
    size_t end = pos;
    while (end < limit && data[end] != 0) ++end;
    return std::string(reinterpret_cast<const char*>(data + pos), end - pos);
}

}  // namespace

bool ElfParser::readLsb32(size_t pos, uint32_t* out) const {
    if (pos > size_ || size_ - pos < 4) return false;
    *out = load32(data_ + pos);
    return true;
}

bool ElfParser::readLsb64(size_t pos, uint64_t* out) const {
    if (pos > size_ || size_ - pos < 8) return false;
    *out = load64(data_ + pos);
    return true;
}

bool ElfParser::parse() {
    parsed_ = false;
    error_.clear();
    sections_.clear();
    programs_.clear();
    symbols_.clear();

    if (size_ < 64) {
        error_ = "file too small to be an ELF64 image";
        return false;
    }
    if (std::memcmp(data_, "\x7f\x45\x4c\x46", 4) != 0) {
        error_ = "bad ELF magic";
        return false;
    }
    if (data_[kEiClass] != 2) {
        error_ = "not an ELF64 file (ELF32 unsupported)";
        return false;
    }
    if (data_[kEiData] != 1) {
        error_ = "only little-endian ELF files are supported";
        return false;
    }

    e_type_ = load16(data_ + kEhType);
    machine_ = load16(data_ + kEhMachine);

    uint64_t phoff = load64(data_ + kEhPhoff);
    uint64_t shoff = load64(data_ + kEhShoff);
    uint16_t phentsize = load16(data_ + kEhPhentsize);
    uint16_t phnum = load16(data_ + kEhPhnum);
    uint16_t shentsize = load16(data_ + kEhShentsize);
    uint16_t shnum = load16(data_ + kEhShnum);
    uint16_t shstrndx = load16(data_ + kEhShstrndx);

    // ---- Program headers (PT_LOAD) -------------------------------------
    size_t phentry = std::max<size_t>(phentsize, 56);
    if (phnum > 0) {
        uint64_t bytes = static_cast<uint64_t>(phentry) * phnum;
        if (phoff <= size_ && bytes <= static_cast<uint64_t>(size_ - phoff)) {
            programs_.reserve(phnum);
            for (uint16_t i = 0; i < phnum && i < 1000; ++i) {
                const uint8_t* p = data_ + phoff + phentry * i;
                ProgramHeader ph{};
                ph.type = load32(p + kPhType);
                ph.flags = load32(p + kPhFlags);
                ph.offset = load64(p + kPhOffset);
                ph.vaddr = load64(p + kPhVaddr);
                ph.filesz = load64(p + kPhFilesz);
                ph.memsz = load64(p + kPhMemsz);
                ph.align = load64(p + kPhAlign);
                programs_.push_back(ph);
            }
        } else if (phnum != 0) {
            error_ = "program header table out of bounds";
        }
    }

    // ---- Section headers -------------------------------------------------
    size_t shentry = std::max<size_t>(shentsize, 64);
    if (shnum > 0) {
        uint64_t bytes = static_cast<uint64_t>(shentry) * shnum;
        if (shoff <= size_ && bytes <= static_cast<uint64_t>(size_ - shoff) &&
            shstrndx < shnum) {
            sections_.reserve(shnum);
            const uint8_t* shstr_base = nullptr;
            size_t shstr_len = 0;
            const uint8_t* shstr_hdr = data_ + shoff + shentry * shstrndx;
            uint64_t str_off = load64(shstr_hdr + kShOffset);
            uint64_t str_size = load64(shstr_hdr + kShSize);
            if (str_off <= size_ &&
                str_size <= static_cast<uint64_t>(size_ - str_off)) {
                shstr_base = data_ + str_off;
                shstr_len = static_cast<size_t>(str_size);
            }
            for (uint16_t i = 0; i < shnum && i < 2000; ++i) {
                const uint8_t* p = data_ + shoff + shentry * i;
                Section s{};
                s.name = (shstr_base && load32(p + kShName) < shstr_len)
                             ? readCString(shstr_base, shstr_len,
                                           load32(p + kShName))
                             : std::string{};
                s.type = load32(p + kShType);
                s.flags = load32(p + kShFlags);
                s.address = load64(p + kShAddr);
                s.offset = load64(p + kShOffset);
                s.size = load64(p + kShSize);
                s.link = load32(p + kShLink);
                s.info = load32(p + kShInfo);
                s.addralign = load64(p + kShAddralign);
                s.entsize = load64(p + kShEntsize);
                sections_.push_back(std::move(s));
            }
        } else if (shnum != 0) {
            error_ = "section header table out of bounds";
        }
    }

    // ---- Dynamic symbols (.dynsym + .dynstr) -----------------------------
    for (const Section& s : sections_) {
        if (s.size == 0 || s.type != kShtDynsym) continue;
        if (s.link >= sections_.size()) continue;
        const Section& strtab = sections_[s.link];
        const uint8_t* strs = nullptr;
        size_t strs_len = 0;
        if (!sectionData(strtab, &strs, &strs_len)) continue;
        const uint8_t* syms = nullptr;
        size_t syms_len = 0;
        if (!sectionData(s, &syms, &syms_len)) continue;

        size_t entry = std::max<size_t>(s.entsize, 24);
        size_t count = syms_len / entry;
        // .dynsym for libminecraftpe is typically a few thousand entries.
        if (count > 1u << 20) count = 1u << 20;
        symbols_.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* p = syms + entry * i;
            uint32_t name = load32(p + kSymName);
            uint8_t info = p[kSymInfo];
            uint16_t shndx = load16(p + kSymShndx);
            uint64_t value = load64(p + kSymValue);
            uint64_t sz = load64(p + kSymSize);
            Symbol sym{};
            sym.value = value;
            sym.size = sz;
            sym.info = info;
            sym.shndx = shndx;
            sym.defined = (shndx != kShnUndef);
            if (shndx != kShnUndef && name < strs_len)
                sym.name = readCString(strs, strs_len, name, 256);
            symbols_.push_back(std::move(sym));
        }
        break;  // first .dynsym is the one we want
    }

    // ---- Base address from the lowest PT_LOAD -----------------------------
    bool have_load = false;
    base_ = 0;
    for (const ProgramHeader& ph : programs_) {
        if (ph.type != kPtLoad) continue;
        uint64_t base = ph.vaddr - ph.offset;
        if (!have_load || base < base_) {
            base_ = base;
            have_load = true;
        }
    }
    if (!have_load) {
        error_ = "no PT_LOAD segments found";
        return false;
    }
    parsed_ = true;
    return true;
}

bool ElfParser::fileOffsetToVaddr(uint64_t file_offset, uint64_t* vaddr) const {
    for (const ProgramHeader& ph : programs_) {
        if (ph.type != kPtLoad) continue;
        if (file_offset >= ph.offset &&
            file_offset < ph.offset + ph.filesz) {
            *vaddr = file_offset - ph.offset + ph.vaddr;
            return true;
        }
    }
    return false;
}

const Section* ElfParser::findSection(const std::string& name) const {
    for (const Section& s : sections_) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

bool ElfParser::sectionData(const Section& s, const uint8_t** ptr,
                            size_t* len) const {
    if (s.size == 0) {
        *ptr = nullptr;
        *len = 0;
        return true;
    }
    if (s.offset > size_ || s.size > static_cast<uint64_t>(size_ - s.offset))
        return false;
    *ptr = data_ + s.offset;
    *len = static_cast<size_t>(s.size);
    return true;
}

}  // namespace glowberry
