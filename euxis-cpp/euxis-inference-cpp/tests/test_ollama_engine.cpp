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

// ---------------------------------------------------------------------------
// supports_model returns false when Ollama is not running
// ---------------------------------------------------------------------------
TEST(OllamaEngineTest, SupportsModelFalseWhenUnreachable) {
    OllamaEngine engine("localhost", 19999);

    EXPECT_FALSE(engine.supports_model("llama3"));
    EXPECT_FALSE(engine.supports_model("nonexistent"));
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

} // anonymous namespace
} // namespace euxis::inference
