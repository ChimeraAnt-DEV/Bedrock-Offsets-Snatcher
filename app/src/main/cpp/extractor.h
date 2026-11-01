// Extraction engine: ties the ELF parser together with the pattern scanner
// and produces the BerryAPI JSON result.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace glowberry {

struct VersionInfo {
    std::string packageName;
    std::string versionName;   // e.g. "1.20.62"
    int versionCode = 0;
};

struct VtableInfo {
    std::string name;   // e.g. "Actor::vtable" or "vtable@0x..."
    uint64_t offset = 0;        // file/vaddr offset of the vtable start
    uint64_t size = 0;          // span, in bytes
    int method_count = 0;
};

struct FunctionOffset {
    std::string name;    // signature id, e.g. "ClientInstance::getLocalPlayer"
    std::string category;
    uint64_t offset = 0; // string offset relative to library base
    int64_t adjust = 0;  // caller-supplied delta applied to offset
    bool verified = false;
};

struct ExtractResult {
    bool ok = false;
    std::string error;
    // Absolute vaddr of the in-memory base (normally 0).
    uint64_t base_address = 0;
    std::string minecraft_version;
    std::vector<FunctionOffset> functions;
    std::vector<VtableInfo> vtables;
    // Number of bytes scanned in each section (for the progress log).
    size_t text_scanned = 0;
    size_t rodata_scanned = 0;
    size_t datarelro_scanned = 0;
};

// Scan `data` (the whole libminecraftpe.so image) for all known signatures.
// `data` must stay alive during the call.
ExtractResult extractOffsets(const uint8_t* data, size_t size,
                             const VersionInfo& version);

}  // namespace glowberry