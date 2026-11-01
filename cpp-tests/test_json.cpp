// Tests of the JSON serialiser (BerryAPI schema).
#include <cstdio>
#include <string>
#include <vector>

#include "extractor.h"
#include "json_gen.h"
#include "test_util.h"

using namespace glowberry;

static ExtractResult sampleResult() {
    ExtractResult r;
    r.ok = true;
    r.base_address = 0;
    r.minecraft_version = "1.20.62";
    r.functions.push_back(
        FunctionOffset{"ClientInstance::getLocalPlayer", "text", 0x10000,
                       /*adjust*/ 0, /*verified*/ true});
    r.functions.push_back(
        FunctionOffset{"Actor::getPos", "text", 0x10100, 0, true});
    r.vtables.push_back(VtableInfo{"vtable@0x360", 0x360, 88, 11});
    return r;
}

TEST(json_contains_schema_fields) {
    std::string j = buildJson(sampleResult());
    EXPECT_TRUE(j.find("\"ok\": true") != std::string::npos);
    EXPECT_TRUE(j.find("\"minecraft_version\": \"1.20.62\"") !=
                std::string::npos);
    EXPECT_TRUE(j.find("\"abi\": \"arm64-v8a\"") != std::string::npos);
    EXPECT_TRUE(j.find("\"base_address\": 0") != std::string::npos);
    EXPECT_TRUE(j.find("\"ClientInstance::getLocalPlayer\"") !=
                std::string::npos);
    EXPECT_TRUE(j.find("\"vtable@0x360\"") != std::string::npos);
    EXPECT_TRUE(j.find("\"method_count\": 11") != std::string::npos);
}

TEST(json_escapes_quotes) {
    ExtractResult r;
    r.ok = false;
    r.error = "bad \"quote\" and\nnewline";
    std::string j = buildJson(r);
    EXPECT_TRUE(j.find("\\\"quote\\\"") != std::string::npos);
    EXPECT_TRUE(j.find("\\n") != std::string::npos);
}