// JSON serialisation of the extraction result (no JNI dependency, so it is
// host-testable).
#pragma once

#include <string>

#include "extractor.h"

namespace glowberry {

std::string buildJson(const ExtractResult& r);

}  // namespace glowberry
