/// @file
/// @brief Tests for SchemaBuilder + derive_input_schema specialisation.
///
/// Validates the today-available path (manual specialisation via
/// SchemaBuilder) and that the resulting JSON matches the shape the
/// hand-written schemas in docs/examples/cpp/tool_calling_loop/ already
/// use. When a compiler ships __cpp_lib_reflection these tests should
/// pass unchanged against the default template.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "euxis/runtime/reflect_schema.hpp"
#include "euxis/runtime/tool_manifest.hpp"

namespace euxis::runtime {
namespace {

// ---------------------------------------------------------------------------
// json_type_for primitive table
// ---------------------------------------------------------------------------
TEST(JsonTypeFor, MapsPrimitivesCorrectly) {
    EXPECT_EQ(json_type_for<bool>(),         "boolean");
    EXPECT_EQ(json_type_for<int>(),          "integer");
    EXPECT_EQ(json_type_for<std::int64_t>(), "integer");
    EXPECT_EQ(json_type_for<std::size_t>(),  "integer");
    EXPECT_EQ(json_type_for<float>(),        "number");
    EXPECT_EQ(json_type_for<double>(),       "number");
    EXPECT_EQ(json_type_for<std::string>(),  "string");
    EXPECT_EQ(json_type_for<const char*>(),  "string");
    EXPECT_EQ(json_type_for<std::vector<int>>(), "array");
}

TEST(JsonTypeFor, UnknownTypeFallsBackToObject) {
    struct Opaque {};
    EXPECT_EQ(json_type_for<Opaque>(), "object");
}

// ---------------------------------------------------------------------------
// SchemaBuilder shape
// ---------------------------------------------------------------------------
TEST(SchemaBuilder, EmitsExpectedJsonObjectShape) {
    const auto schema = SchemaBuilder{}
        .field<std::string>("path", "Directory to scan")
        .field<int>("depth")
        .required("path")
        .build();

    ASSERT_TRUE(schema.is_object());
    EXPECT_EQ(schema["type"], "object");
    ASSERT_TRUE(schema["properties"].is_object());
    EXPECT_EQ(schema["properties"]["path"]["type"], "string");
    EXPECT_EQ(schema["properties"]["path"]["description"], "Directory to scan");
    EXPECT_EQ(schema["properties"]["depth"]["type"], "integer");
    EXPECT_FALSE(schema["properties"]["depth"].contains("description"));
    ASSERT_TRUE(schema["required"].is_array());
    EXPECT_EQ(schema["required"][0], "path");
}

TEST(SchemaBuilder, OmitsRequiredKeyWhenEmpty) {
    const auto schema = SchemaBuilder{}.field<int>("x").build();
    EXPECT_FALSE(schema.contains("required"));
}

// ---------------------------------------------------------------------------
// derive_input_schema specialisation — mirrors the hand-written schema
// from docs/examples/cpp/tool_calling_loop/main.cpp for the `scan` tool.
// ---------------------------------------------------------------------------
struct ScanArgs {
    std::string path;
    int         depth;
};

} // namespace
} // namespace euxis::runtime

template<>
auto euxis::runtime::derive_input_schema<euxis::runtime::ScanArgs>()
    -> nlohmann::json {
    return euxis::runtime::SchemaBuilder{}
        .field<std::string>("path")
        .field<int>("depth")
        .required("path")
        .build();
}

namespace euxis::runtime {
namespace {

TEST(DeriveInputSchema, SpecialisationMatchesHandWrittenShape) {
    const auto derived = derive_input_schema<ScanArgs>();
    EXPECT_EQ(derived["type"], "object");
    EXPECT_EQ(derived["properties"]["path"]["type"],  "string");
    EXPECT_EQ(derived["properties"]["depth"]["type"], "integer");
    EXPECT_EQ(derived["required"][0], "path");
}

// Round-trip: the derived schema slots into a ToolDeclaration_v2 the same
// way the hand-written one does. This is what SDK consumers do.
TEST(DeriveInputSchema, FillsToolDeclarationV2InputSchema) {
    ToolDeclaration_v2 decl;
    decl.name         = "scan";
    decl.description  = "Scan a path for findings";
    decl.input_schema = derive_input_schema<ScanArgs>();

    EXPECT_EQ(decl.input_schema["type"], "object");
    EXPECT_TRUE(decl.input_schema["properties"].contains("path"));
    EXPECT_TRUE(decl.input_schema["properties"].contains("depth"));
    EXPECT_EQ(classify_approval(decl), ApprovalClass::Readonly);  // "scan" verb
}

} // namespace
} // namespace euxis::runtime
