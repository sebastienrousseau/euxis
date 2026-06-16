#include <gtest/gtest.h>

#include <cstdlib>

#include "euxis/inference/llama_engine.hpp"

namespace euxis::inference {
namespace {

/// Tests assume there's no llama-server listening. Localhost
/// port 8080 (the default) is commonly held by other dev tooling
/// (e.g. python -m http.server, dashboards). Pinning the engine
/// to an ephemeral high port via the documented env-var override
/// keeps the test deterministic regardless of the host's port
/// landscape.
class LlamaEnvFixture {
public:
    LlamaEnvFixture() {
        ::setenv("LLAMA_SERVER_HOST", "127.0.0.1", 1);
        ::setenv("LLAMA_SERVER_PORT", "1",         1);
    }
    ~LlamaEnvFixture() {
        ::unsetenv("LLAMA_SERVER_HOST");
        ::unsetenv("LLAMA_SERVER_PORT");
    }
};

namespace { LlamaEnvFixture g_llama_env; } // file-scope auto-init

// ---------------------------------------------------------------------------
// Constructor works without crashing
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, ConstructorWorks) {
    LocalModelConfig cfg;
    cfg.model_name   = "test-model";
    cfg.model_path   = "/tmp/nonexistent.gguf";
    cfg.context_size = 2048;

    EXPECT_NO_THROW({ LlamaEngine engine(cfg); });
}

// ---------------------------------------------------------------------------
// Generate returns error when llama-server is unavailable
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, GenerateReturnsErrorWhenServerUnavailable) {
    LocalModelConfig cfg;
    cfg.model_name = "stub-model";

    LlamaEngine engine(cfg);
    auto result = engine.generate("Hello, world!", 64);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("not reachable") != std::string::npos);
}

// ---------------------------------------------------------------------------
// supports_model matches config when server is unavailable
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, SupportsModelMatchesConfig) {
    LocalModelConfig cfg;
    cfg.model_name = "llama3-8b";

    LlamaEngine engine(cfg);

    EXPECT_TRUE(engine.supports_model("llama3-8b"));
    EXPECT_FALSE(engine.supports_model("mistral-7b"));
    EXPECT_FALSE(engine.supports_model(""));
}

// ---------------------------------------------------------------------------
// health() returns valid JSON with expected keys
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, HealthReturnsJson) {
    LocalModelConfig cfg;
    cfg.model_name   = "health-test";
    cfg.context_size = 4096;

    LlamaEngine engine(cfg);
    auto h = engine.health();

    EXPECT_TRUE(h.is_object());
    EXPECT_EQ(h["engine"], "llama.cpp");
    EXPECT_EQ(h["status"], "unreachable");
    EXPECT_EQ(h["model"], "health-test");
    EXPECT_EQ(h["context_size"], 4096u);
}

// ---------------------------------------------------------------------------
// health() contains connection info (host and port)
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, HealthContainsConnectionInfo) {
    LocalModelConfig cfg;
    cfg.model_name = "conn-test";

    LlamaEngine engine(cfg);
    auto h = engine.health();

    EXPECT_TRUE(h.contains("host"));
    EXPECT_TRUE(h.contains("port"));
    EXPECT_EQ(h["host"], "127.0.0.1");
    // Port comes from the LLAMA_SERVER_PORT env override applied
    // by the file-scope LlamaEnvFixture above (was 8080 default).
    EXPECT_EQ(h["port"], 1);
}

// ---------------------------------------------------------------------------
// Move construction works
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, MoveConstruction) {
    LocalModelConfig cfg;
    cfg.model_name = "movable";

    LlamaEngine a(cfg);
    LlamaEngine b(std::move(a));

    EXPECT_TRUE(b.supports_model("movable"));
    auto h = b.health();
    EXPECT_EQ(h["model"], "movable");
}

// ---------------------------------------------------------------------------
// Move assignment works
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, MoveAssignment) {
    LocalModelConfig cfg_a;
    cfg_a.model_name = "model-a";

    LocalModelConfig cfg_b;
    cfg_b.model_name = "model-b";

    LlamaEngine a(cfg_a);
    LlamaEngine b(cfg_b);

    b = std::move(a);
    EXPECT_TRUE(b.supports_model("model-a"));
}

// ---------------------------------------------------------------------------
// Generate with various max_tokens values
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, GenerateWithDifferentMaxTokens) {
    LocalModelConfig cfg;
    cfg.model_name = "token-test";
    LlamaEngine engine(cfg);

    auto result1 = engine.generate("test", 1);
    ASSERT_FALSE(result1.has_value());

    auto result2 = engine.generate("test", 4096);
    ASSERT_FALSE(result2.has_value());

    // Both should fail with "not reachable"
    EXPECT_NE(result1.error().find("not reachable"), std::string::npos);
    EXPECT_NE(result2.error().find("not reachable"), std::string::npos);
}

// ---------------------------------------------------------------------------
// supports_model with empty string
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, SupportsModelEmptyString) {
    LocalModelConfig cfg;
    cfg.model_name = "my-model";
    LlamaEngine engine(cfg);
    // Empty name should not match config model_name
    EXPECT_FALSE(engine.supports_model(""));
}

// ---------------------------------------------------------------------------
// health() with different config values
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, HealthWithLargeContextSize) {
    LocalModelConfig cfg;
    cfg.model_name = "large-ctx";
    cfg.context_size = 131072;
    LlamaEngine engine(cfg);
    auto h = engine.health();
    EXPECT_EQ(h["context_size"], 131072u);
    EXPECT_EQ(h["status"], "unreachable");
}

// ---------------------------------------------------------------------------
// Config with temperature and top_p
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, ConfigWithCustomTemperature) {
    LocalModelConfig cfg;
    cfg.model_name = "temp-model";
    cfg.temperature = 0.0f;
    cfg.top_p = 0.5f;
    LlamaEngine engine(cfg);

    // Generate should build a body with the custom temperature
    auto result = engine.generate("Hello", 32);
    ASSERT_FALSE(result.has_value());
    // Fails because server is down, but config was used
}

// ---------------------------------------------------------------------------
// Repeated health checks return consistent results
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, RepeatedHealthChecks) {
    LocalModelConfig cfg;
    cfg.model_name = "repeat-check";
    LlamaEngine engine(cfg);

    auto h1 = engine.health();
    auto h2 = engine.health();
    EXPECT_EQ(h1["engine"], h2["engine"]);
    EXPECT_EQ(h1["model"], h2["model"]);
    EXPECT_EQ(h1["status"], h2["status"]);
}

// ---------------------------------------------------------------------------
// Generate after health check returns consistent error
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, GenerateAfterHealthCheck) {
    LocalModelConfig cfg;
    cfg.model_name = "sequence-test";
    LlamaEngine engine(cfg);

    auto h = engine.health();
    EXPECT_EQ(h["status"], "unreachable");

    auto r = engine.generate("test prompt", 100);
    ASSERT_FALSE(r.has_value());
}

TEST(LlamaEngineTest, EpisodicGenerateFailsGracefully) {
    LocalModelConfig cfg;
    cfg.model_name = "episodic-test";
    LlamaEngine engine(cfg);
    
    auto get_episodes = []() -> std::vector<euxis::runtime::SessionMessage> {
        return {{.role = euxis::runtime::Role::Assistant, .content = "Trace", .agent_id = {}, .model = {}, .timestamp = {}, .duration_ms = 0.0, .decision_trace_hash = {}}};
    };

    auto result = engine.episodic_generate(get_episodes(), "System prompt", 32);
    ASSERT_FALSE(result.has_value());
}

} // anonymous namespace
} // namespace euxis::inference
