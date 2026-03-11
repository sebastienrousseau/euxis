#include <gtest/gtest.h>

#include "euxis/inference/ollama_engine.hpp"

namespace euxis::inference {
namespace {

// ---------------------------------------------------------------------------
// Constructor with defaults
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, ConstructorDefaults) {
    EXPECT_NO_THROW({ OllamaEngine engine; });
}

// ---------------------------------------------------------------------------
// Constructor with custom host/port
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, ConstructorCustom) {
    EXPECT_NO_THROW({
        OllamaEngine engine("192.168.1.100", 8080);
    });
}

// ---------------------------------------------------------------------------
// health() returns JSON even when Ollama is not running
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, HealthReturnsJsonWhenUnreachable) {
    // Use a port that is almost certainly not running Ollama
    OllamaEngine engine("localhost", 19999);

    auto h = engine.health();

    EXPECT_TRUE(h.is_object());
    EXPECT_EQ(h["engine"], "ollama");
    EXPECT_EQ(h["host"], "localhost");
    EXPECT_EQ(h["port"], 19999);
    // Either "unreachable" or "error" — both are acceptable
    EXPECT_TRUE(h.contains("status"));
    auto status = h["status"].get<std::string>();
    EXPECT_TRUE(status == "unreachable" || status == "error")
        << "Unexpected status: " << status;
}

TEST(OllamaEngineTest, HealthReturnsErrorField) {
    OllamaEngine engine("localhost", 19999);

    auto h = engine.health();
    auto status = h["status"].get<std::string>();
    if (status == "unreachable") {
        EXPECT_TRUE(h.contains("error"));
        EXPECT_FALSE(h["error"].get<std::string>().empty());
    }
}

// ---------------------------------------------------------------------------
// generate() fails gracefully when Ollama is not running
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, GenerateFailsGracefully) {
    OllamaEngine engine("localhost", 19999);

    auto result = engine.generate("Hello", 32);

    ASSERT_FALSE(result.has_value());
    // Error message should mention the failure
    EXPECT_FALSE(result.error().empty());
}

TEST(OllamaEngineTest, GenerateDefaultMaxTokens) {
    OllamaEngine engine("localhost", 19999);

    // Test with default max_tokens parameter
    auto result = engine.generate("Hello");
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST(OllamaEngineTest, GenerateWithLargeMaxTokens) {
    OllamaEngine engine("localhost", 19999);

    auto result = engine.generate("Hello", 4096);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// supports_model returns false when Ollama is not running
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, SupportsModelFalseWhenUnreachable) {
    OllamaEngine engine("localhost", 19999);

    EXPECT_FALSE(engine.supports_model("llama3"));
    EXPECT_FALSE(engine.supports_model("nonexistent"));
}

TEST(OllamaEngineTest, SupportsModelEmptyName) {
    OllamaEngine engine("localhost", 19999);
    EXPECT_FALSE(engine.supports_model(""));
}

// ---------------------------------------------------------------------------
// make_request fails gracefully with bad host
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, MakeRequestBadHost) {
    OllamaEngine engine("localhost", 19999);

    // generate internally calls make_request; error is propagated
    auto result = engine.generate("test prompt", 10);
    ASSERT_FALSE(result.has_value());
    auto err = result.error();
    // Error should mention HTTP or connection failure
    EXPECT_FALSE(err.empty());
}

// ---------------------------------------------------------------------------
// Integration: if Ollama IS running on default port, health returns ok
// (This test passes whether or not Ollama is actually available)
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, HealthIntegration) {
    OllamaEngine engine; // default localhost:11434

    auto h = engine.health();
    EXPECT_TRUE(h.is_object());
    EXPECT_EQ(h["engine"], "ollama");

    auto status = h["status"].get<std::string>();
    // Either "ok" (Ollama running) or "unreachable" (not running) — both pass
    EXPECT_TRUE(status == "ok" || status == "unreachable" ||
                status == "error")
        << "Unexpected status: " << status;
}

TEST(OllamaEngineTest, HealthStructure) {
    OllamaEngine engine("localhost", 19999);

    auto h = engine.health();
    // Verify all expected fields are present
    EXPECT_TRUE(h.contains("engine"));
    EXPECT_TRUE(h.contains("host"));
    EXPECT_TRUE(h.contains("port"));
    EXPECT_TRUE(h.contains("status"));
}

} // anonymous namespace
} // namespace euxis::inference
