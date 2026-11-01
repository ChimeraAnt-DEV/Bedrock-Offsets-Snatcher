#include "json_gen.h"

#include <cstdio>
#include <string>

namespace glowberry {

static void jsonEscape(std::string* out, const std::string& in) {
    for (unsigned char c : in) {
        switch (c) {
            case '"': *out += "\\\""; break;
            case '\\': *out += "\\\\"; break;
            case '\n': *out += "\\n"; break;
            case '\r': *out += "\\r"; break;
            case '\t': *out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char tmp[8];
                    std::snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    *out += tmp;
                } else {
                    *out += static_cast<char>(c);
                }
        }
    }
}

std::string buildJson(const ExtractResult& r) {
    std::string out;
    out.reserve(4096);
    out += "{\n";
    out += "  \"ok\": ";
    out += r.ok ? "true" : "false";
    if (!r.ok) {
        out += ",\n  \"error\": \"";
        jsonEscape(&out, r.error);
        out += "\"";
    }
    out += ",\n  \"minecraft_version\": \"";
    jsonEscape(&out, r.minecraft_version);
    out += "\",\n  \"abi\": \"arm64-v8a\",\n  \"base_address\": ";
    out += std::to_string(r.base_address);

    out += ",\n  \"functions\": [";
    bool first = true;
    for (const FunctionOffset& f : r.functions) {
        if (!first) out += ",";
        first = false;
        out += "\n    {\"name\": \"";
        jsonEscape(&out, f.name);
        out += "\", \"category\": \"";
        out += f.category;
        out += "\", \"offset\": ";
        out += std::to_string(f.offset);
        out += ", \"verified\": ";
        out += f.verified ? "true" : "false";
        out += ", \"adjust\": ";
        out += std::to_string(static_cast<long long>(f.adjust));
        out += "}";
    }
    out += "\n  ]";

    out += ",\n  \"vtables\": [";
    first = true;
    for (const VtableInfo& v : r.vtables) {
        if (!first) out += ",";
        first = false;
        out += "\n    {\"name\": \"";
        jsonEscape(&out, v.name);
        out += "\", \"offset\": ";
        out += std::to_string(v.offset);
        out += ", \"size\": ";
        out += std::to_string(v.size);
        out += ", \"method_count\": ";
        out += std::to_string(v.method_count);
        out += "}";
    }
    out += "\n  ]\n}";
    return out;
}



}  // namespace glowberry
