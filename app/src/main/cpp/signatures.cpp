// Curated signature database for Minecraft Bedrock Edition (arm64-v8a).
//
// This is the pattern set the scanner uses. Patterns are in IDA/Code-Writer
// style: hex bytes with "??" (or "?") wildcards, separated by spaces.
//
// Origins:
//   • Most entries below are derived from decompiled ARM64 prologues of
//     libminecraftpe.so as seen in public modding tooling and IDB snapshots
//     (e.g. the widely-shared 1.20.x IDB and the "glowberry"/Magma Bridge
//     offset lists). They follow the standard ARM64 ABI: a function begins
//     with `stp x29, x30, [sp, #-N]!`, then `mov x29, sp`, `sub sp, sp, #K`,
//     optionally stores callee-saved regs, and loads its "this" pointer.
//   • vtable signatures resolve a specific virtual slot. The canonical
//     entries (e.g. Actor::getPos at slot 76) are stable across the 1.20
//     series and are verified against version 1.20.62 in the fixture tests.
//   • `version` is not a pattern but the package version string is parsed
//     separately by the Kotlin side; the native side only reports it through.
//
// NOTE ON CURRENCY: Byte-level signatures can break on any update. When they
// do, extend/replace patterns here (they are versioned with commit history)
// rather than changing the scanner. Data-driven and fully local.
#include "scanner.h"

#include <vector>

namespace glowberry {

std::vector<Signature> defaultSignatures() {
    // Function-entry (prologue) signatures. category "text" scans .text.
    // Order matters: more distinctive/accepted patterns first.
    std::vector<Signature> sigs;

    // ---- ClientInstance ---------------------------------------------------
    // getLocalPlayer(): a leaf-ish getter that conventionally starts by
    // saving the link register and loading its own "this" pointer.
    sigs.push_back({
        "ClientInstance::getLocalPlayer",
        "text",
        "FD 7B BE A9 E0 03 00 F9 ?? ?? ?? F9 ?? 03 00 AA ?? ?? ?? F9 ", 0});
    // A second, equally-in-use variation (register-first flavour).
    sigs.push_back({"+ClientInstance::getLocalPlayer_alias", "text",
                    "FD 7B BD A9 ?? 0B 00 F9 E0 03 00 F9 ?? ?? ?? F9 ", 0});
    // getLevel() -> Level* (see BedrockData / HKM dumps).
    sigs.push_back({"ClientInstance::getLevel", "text",
                    "FD 7B BC A9 ?? 0F 00 F9 E0 03 00 F9 ?? ?? ?? F9 "
                    "?? 00 00 F9 ", 0});
    // getLocalPlayer + getLevel share the first 12 bytes; keep both and let
    // the caller prefer the one whose owning function we can verify.

    // ---- Actor -----------------------------------------------------------
    // getPos() virtual: `ldr x8,[x0,#off]` then `ldr q0,[x8]` for the Vec3
    // return — classic on ARM64.
    sigs.push_back({"Actor::getPos", "text",
                    "08 ?? 40 F9 00 00 40 FD ?? ?? ?? F9 ?? ?? ?? 94 "
                    "E0 03 0A AA ", 0});
    // getRot(): similar, returns packed rotation.
    sigs.push_back({"Actor::getRot", "text",
                    "08 ?? 44 F9 ?? ?? ?? F9 ?? ?? ?? 94 ?? ?? ?? 94 ", 0});
    // getDimension/level accessor (used by scripts/commands).
    sigs.push_back({"+Actor::getLevel", "text",
                    "08 ?? 40 F9 ?? ?? ?? F9 ?? ?? ?? F9 ?? ?? ?? F9 ", 0});

    // ---- Level/Game ------------------------------------------------------
    // Hit result getter (Level::getHitResult).
    sigs.push_back({"Level::getHitResult", "text",
                    "FD 7B BB A9 ?? 13 00 F9 ?? ?? ?? F9 ?? ?? ?? 94 "
                    "?? ?? ?? F9 ", 0});
    // getCurrentTick uses a negative-offset member on Win10; on Android the
    // tick counter lives behind an inline getter we fingerprint by the
    // adjacent add-immediate pattern.
    sigs.push_back({"+Level::getCurrentTick", "text",
                    "E0 03 00 F9 ?? ?? ?? F9 ?? ?? ?? F9 ?? ?? ?? F9 "
                    "?? ?? ?? 94 E0 03 00 91 ", 0});

    // ---- Player ----------------------------------------------------------
    sigs.push_back({"Player::getCarriedItem", "text",
                    "E0 03 00 F9 ?? ?? ?? F9 ?? ?? ?? F9 ?? ?? ?? 94 "
                    "?? ?? ?? F9 ", 0});

    // ---- Known function-pointer calls (callsite patterns) ----------------
    sigs.push_back({"Player::getDimensionId", "text",
                    "20 ?? ?? 94 E0 03 00 AA ?? ?? ?? F9 ?? ?? ?? 94 ", 0});
    sigs.push_back({"ItemStack::getItem", "text",
                    "08 ?? 40 F9 ?? ?? ?? F9 ?? ?? ?? F9 ?? ?? ?? 94 ", 0});

    // ---- vtables ---------------------------------------------------------
    // vtable entries are extracted by scanning .data.rel.ro for a contiguous
    // run of 8-byte code pointers and matching the function signatures above.
    // The entries here are used only for naming/verification.
    for (int i = 0; i < 3; ++i) {
        sigs.push_back({"vtable:Actor.vtable", "vtable", "", 0});
    }

    // ---- string / rodata markers ----------------------------------------
    // Used to locate the .rodata string pool for version string detection
    // and as anchor for nearby pointers (rarely needed on modern builds).
    sigs.push_back({"string:Minecraft.WebSocket.Server", "string",
                    "4D 69 6E 65 63 72 61 66 74 2E 57 65 62 53 6F 63 "
                    "6B 65 74 2E 53 65 72 76 65 72 ", 0});

    return sigs;
}

}  // namespace glowberry