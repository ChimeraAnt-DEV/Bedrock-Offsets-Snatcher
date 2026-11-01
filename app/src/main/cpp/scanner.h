// High-speed pattern scanner with wildcard support.
//
// Patterns are expressed as byte strings using two-byte hexadecimal tokens
// separated by spaces; the token "??" (or '?' '?') matches any byte, e.g.
//     "FD 7B BB A9 ?? ?? ?? ?? E0 03 00 F9"
//
// Search strategy ("QGram jump"): take the two first *fixed* bytes of the
// pattern and build a 65536-entry shift table. The hot loop reads a 2-byte
// bigram from the haystack and skips ahead by the stored distance whenever it
// cannot start a match, which gives near-Boyer-Moore skipping for the common
// case that the leading fixed bytes are rare, while keeping exact, simple
// code. The tail of a candidate is then verified byte-by-byte honouring
// wildcards.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace glowberry {

// A compiled signature search pattern.
struct Signature {
    std::string id;       // YAML/JSON id, e.g. "ClientInstance::getLocalPlayer"
    std::string category; // "function", "vtable", "string"
    // The pattern for .text/.rodata matching; empty for vtable extraction.
    std::string pattern;
    // Extra sign-extended reinterpretation hints (unused for raw scan).
    // Signed delta to apply to the match address to obtain the published
    // value (e.g. +1 for a Thumb-style label on some architectures).
    int64_t adjust = 0;
};

struct SignatureMatch {
    size_t pattern_index = 0;
    uint64_t vaddr = 0;   // address of the match, relative to base
    uint64_t file_offset = 0;
};

class SignatureScanner {
public:
    SignatureScanner(const uint8_t* haystack, size_t haystack_len,
                     uint64_t haystack_start_vaddr, uint64_t base);

    // All signatures matching at least once; reports the first occurrence of
    // each pattern (enough for uniqueness-checked functions).
    bool run(const std::vector<Signature>& sigs,
             std::vector<SignatureMatch>* matches,
             std::string* error);

    // Raw scan of a single pattern (used by tests and string lookups).
    // Returns true and fills *pos on the first hit.
    static bool findFirst(const uint8_t* haystack, size_t haystack_len,
                          const std::string& hex_pattern, size_t* pos,
                          size_t* pattern_len);

private:
    const uint8_t* h_;
    size_t n_;
    uint64_t start_vaddr_;
    uint64_t base_;
};

// Parse a hex token stream like "FD 7B ?? A9" into (bytes, wildcard_flags).
// Accepts '?', '??', "." tokens as wildcards, and both cases of hex digits.
void compilePattern(const std::string& text, std::vector<uint8_t>* bytes,
                    std::vector<bool>* wildcard);

}  // namespace glowberry