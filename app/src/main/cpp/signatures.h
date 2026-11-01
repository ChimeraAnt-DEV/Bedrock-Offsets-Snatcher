// Signature database for Minecraft Bedrock Edition (arm64-v8a).
#pragma once

#include "scanner.h"

#include <vector>

namespace glowberry {

// The built-in pattern set for libminecraftpe.so. Data-driven; edit this file
// to add/refresh signatures for future game versions.
std::vector<Signature> defaultSignatures();

}  // namespace glowberry