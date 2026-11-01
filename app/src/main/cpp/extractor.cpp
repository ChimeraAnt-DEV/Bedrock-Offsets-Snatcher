#include "extractor.h"

#include "elf_parser.h"
#include "scanner.h"
#include <cstdio>
#include "signatures.h"

namespace glowberry {
namespace {

inline uint64_t readLe64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

// True when `addr` is a plausible AArch64 code pointer inside .text.
bool isTextPointer(const ElfParser& parser, const Section* text,
                   uint64_t addr) {
    const uint64_t kLowBits = 0xfffffffffull;  // 36-bit address space
    addr &= kLowBits;

    // Prefer the canonical address: the ELF image is usually loaded at the
    // vaddr corresponding to file offset 0 (that is also our base address).
    if (addr >= parser.baseAddress()) {
        addr -= parser.baseAddress();
    }
    if (!text || text->size == 0) return false;
    uint64_t addr_lo = text->address;
    uint64_t addr_hi = addr_lo + text->size;
    return addr >= addr_lo && addr < addr_hi;
}

}  // namespace

ExtractResult extractOffsets(const uint8_t* data, size_t size,
                             const VersionInfo& version) {
    ExtractResult result;
    result.minecraft_version = version.versionName;

    ElfParser parser(data, size);
    if (!parser.parse()) {
        result.error = "ELF parse failed: " + parser.error();
        return result;
    }
    result.base_address = parser.baseAddress();

    const Section* text = parser.findSection(".text");
    const Section* rodata = parser.findSection(".rodata");
    const Section* data_rel_ro = parser.findSection(".data.rel.ro");

    // ---- 1) Pattern scan across the interesting sections -----------------
    std::vector<Signature> sigs = defaultSignatures();
    std::vector<Signature> text_sigs, rodata_sigs;
    for (const Signature& s : sigs) {
        if (s.category == "vtable") continue;
        if (s.category == "string")
            rodata_sigs.push_back(s);
        else
            text_sigs.push_back(s);
    }

    if (text) {
        const uint8_t* p = nullptr;
        size_t len = 0;
        if (parser.sectionData(*text, &p, &len)) {
            SignatureScanner sc(p, len, text->address, parser.baseAddress());
            std::vector<SignatureMatch> m;
            std::string err;
            if (sc.run(text_sigs, &m, &err)) {
                result.text_scanned = len;
                for (const SignatureMatch& sm : m) {
                    const Signature& sig = text_sigs[sm.pattern_index];
                    FunctionOffset fo{};
                    fo.name = sig.id;
                    fo.category = "text";
                    fo.offset = sm.vaddr - parser.baseAddress() + sig.adjust;
                    fo.adjust = sig.adjust;
                    fo.verified = true;
                    result.functions.push_back(fo);
                }
            } else {
                result.error = err;
            }
        }
    }

    if (rodata) {
        const uint8_t* p = nullptr;
        size_t len = 0;
        if (parser.sectionData(*rodata, &p, &len)) {
            SignatureScanner sc(p, len, rodata->address,
                               parser.baseAddress());
            std::vector<SignatureMatch> m;
            std::string err;
            if (sc.run(rodata_sigs, &m, &err)) {
                result.rodata_scanned = len;
                for (const SignatureMatch& sm : m) {
                    const Signature& sig = rodata_sigs[sm.pattern_index];
                    FunctionOffset fo{};
                    fo.name = sig.id;
                    fo.category = "rodata";
                    fo.offset = sm.vaddr - parser.baseAddress() + sig.adjust;
                    fo.adjust = sig.adjust;
                    fo.verified = true;
                    result.functions.push_back(fo);
                }
            }
        }
    }

    // ---- 2) vtable extraction from .data.rel.ro --------------------------
    // A vtable is an 8-byte-aligned run of pointers, each of which denotes a
    // function inside .text. Runs shorter than kMinSlots are treated as data.
    if (data_rel_ro) {
        const uint8_t* p = nullptr;
        size_t len = 0;
        if (parser.sectionData(*data_rel_ro, &p, &len)) {
            result.datarelro_scanned = len;
            const size_t kPtr = 8;
            const size_t kMinSlots = 4;
            const uint64_t base = parser.baseAddress();
            uint64_t first_vaddr = 0;
            if (!parser.fileOffsetToVaddr(data_rel_ro->offset, &first_vaddr))
                first_vaddr = data_rel_ro->address;

            size_t i = 0;
            while (i + kPtr <= len) {
                if (i % kPtr != 0) { ++i; continue; }
                uint64_t entry = readLe64(p + i);
                // ARM64 vtables never carry the low "1" marker (no Thumb);
                // mask tag bits anyway for robustness.
                entry &= ~static_cast<uint64_t>(0x3);
                if (!isTextPointer(parser, text, entry)) { i += 8; continue; }

                // Try to extend a run of text pointers.
                size_t n_slots = 1;
                while (i + (n_slots + 1) * kPtr <= len) {
                    uint64_t e = readLe64(p + i + n_slots * kPtr);
                    e &= ~static_cast<uint64_t>(0x3);
                    if (!isTextPointer(parser, text, e)) break;
                    ++n_slots;
                }
                if (n_slots >= kMinSlots) {
                    VtableInfo vt{};
                    vt.offset = first_vaddr + i - base;
                    vt.size = static_cast<uint64_t>(n_slots) * kPtr;
                    vt.method_count = static_cast<int>(n_slots);
                    char name[48];
                    std::snprintf(name, sizeof(name), "vtable@0x%llx",
                                  static_cast<unsigned long long>(vt.offset));
                    vt.name = name;
                    result.vtables.push_back(vt);
                    i += n_slots * kPtr;
                } else {
                    i += 8;
                }
            }
        }
    }

    result.ok = result.error.empty() && !result.functions.empty();
    if (!result.ok && result.error.empty())
        result.error = "no offsets extracted (signature set too new for this APK?)";
    return result;
}

}  // namespace glowberry