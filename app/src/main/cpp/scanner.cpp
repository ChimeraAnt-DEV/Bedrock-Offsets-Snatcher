// High-speed pattern scanner with wildcard support.
//
// Patterns use per-byte tokens: two hex digits or "??" (any byte), separated
// by spaces, e.g.
//     "FD 7B BB A9 ?? ?? ?? 52 E0 03 00 F9"
//
// Algorithm: the pattern is compiled once into raw bytes plus a wildcard mask.
// The first *fixed* byte (skipping leading wildcards) is located with memchr,
// and every occurrence is a candidate start for the full pattern, which is
// then verified byte-by-byte honouring wildcards. Fixed bytes in code appear
// with probability ~1/256, so memchr skipping keeps the scan fast while the
// verification step is trivially correct.
#include "scanner.h"

#include <cstdint>
#include <cstring>

namespace glowberry {
namespace {

constexpr size_t kNoAnchor = static_cast<size_t>(-1);

inline int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Find the first fixed (non-wildcard) byte index.
size_t firstFixed(const std::vector<uint8_t>& bytes,
                  const std::vector<bool>& wildcard) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (!wildcard[i]) return i;
    }
    return kNoAnchor;
}

}  // namespace

void compilePattern(const std::string& text, std::vector<uint8_t>* bytes,
                    std::vector<bool>* wildcard) {
    bytes->clear();
    wildcard->clear();
    size_t i = 0;
    const size_t ne = text.size();
    while (i < ne) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == ',' || c == ';' || c == '\n' ||
            c == '\r') {
            ++i;
            continue;
        }
        if (c == '?' || c == '.') {
            if (i + 1 < ne && (text[i + 1] == '?' || text[i + 1] == '.'))
                i += 2;
            else
                ++i;
            bytes->push_back(0);
            wildcard->push_back(true);
            continue;
        }
        int hi = hexDigit(c);
        if (hi < 0) {
            // Unknown token: treat as a single wildcard so scanning does not
            // silently produce wrong results for a malformed config entry.
            bytes->push_back(0);
            wildcard->push_back(true);
            ++i;
            continue;
        }
        int lo = 0;
        if (i + 1 < ne && hexDigit(text[i + 1]) >= 0) {
            lo = hexDigit(text[i + 1]);
            ++i;
        }
        bytes->push_back(static_cast<uint8_t>((hi << 4) | lo));
        wildcard->push_back(false);
        ++i;
    }
}

SignatureScanner::SignatureScanner(const uint8_t* haystack, size_t haystack_len,
                                   uint64_t start_vaddr, uint64_t base)
    : h_(haystack), n_(haystack_len), start_vaddr_(start_vaddr), base_(base) {}

bool SignatureScanner::findFirst(const uint8_t* haystack, size_t haystack_len,
                                 const std::string& hex_pattern, size_t* pos,
                                 size_t* pattern_len) {
    std::vector<uint8_t> bytes;
    std::vector<bool> wc;
    compilePattern(hex_pattern, &bytes, &wc);
    const size_t pat = bytes.size();
    if (pat == 0 || pat > haystack_len) return false;

    const size_t anchor = firstFixed(bytes, wc);
    if (anchor == kNoAnchor) {
        // All wildcards: matches from offset 0.
        if (pos) *pos = 0;
        if (pattern_len) *pattern_len = pat;
        return true;
    }

    const uint8_t needle = bytes[anchor];
    const uint8_t* hay_end = haystack + haystack_len;
    const uint8_t* scan = haystack;
    for (;;) {
        const void* found = std::memchr(scan, needle,
                                        static_cast<size_t>(hay_end - scan));
        if (!found) return false;
        size_t needle_off = static_cast<size_t>(
            static_cast<const uint8_t*>(found) - haystack);
        // The pattern must start earlier so that byte `anchor` aligns.
        if (needle_off >= anchor) {
            size_t start = needle_off - anchor;
            if (start + pat <= haystack_len) {
                bool ok = true;
                for (size_t j = 0; j < pat; ++j) {
                    // Never read past the caller-provided range.
                    if (static_cast<size_t>(start + j) >= haystack_len) {
                        ok = false;
                        break;
                    }
                    if (!wc[j] && haystack[start + j] != bytes[j]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    if (pos) *pos = start;
                    if (pattern_len) *pattern_len = pat;
                    return true;
                }
            }
        }
        scan = static_cast<const uint8_t*>(found) + 1;
    }
}

bool SignatureScanner::run(const std::vector<Signature>& sigs,
                           std::vector<SignatureMatch>* matches,
                           std::string* error) {
    matches->clear();
    size_t found = 0;
    for (size_t i = 0; i < sigs.size(); ++i) {
        const Signature& sig = sigs[i];
        if (sig.pattern.empty()) continue;  // e.g. pure-vtable entries

        size_t file_off = 0;
        size_t plen = 0;
        if (!findFirst(h_, n_, sig.pattern, &file_off, &plen)) continue;

        SignatureMatch m{};
        m.pattern_index = i;
        m.file_offset = file_off;
        m.vaddr = start_vaddr_ + file_off - base_;
        matches->push_back(m);
        ++found;
    }
    if (error) {
        // Report the number of misses without failing the whole scan.
        if (found == 0) {
            *error = "no signatures matched";
        } else if (found < sigs.size()) {
            *error = "";
        }
    }
    return found > 0;
}

}  // namespace glowberry