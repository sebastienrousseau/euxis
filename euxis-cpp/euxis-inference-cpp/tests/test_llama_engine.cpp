#include <gtest/gtest.h>

#include "euxis/inference/llama_engine.hpp"

namespace euxis::inference {
namespace {

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
// Generate returns stub error
// ---------------------------------------------------------------------------
TEST(LlamaEngineTest, GenerateReturnsStubError) {
    LocalModelConfig cfg;
    cfg.model_name = "stub-model";

    LlamaEngine engine(cfg);
    auto result = engine.generate("Hello, world!", 64);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "llama.cpp not linked");
}

// ---------------------------------------------------------------------------
// supports_model matches config
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
    EXPECT_EQ(h["status"], "stub");
    EXPECT_EQ(h["loaded"], false);
    EXPECT_EQ(h["model"], "health-test");
    EXPECT_EQ(h["context_size"], 4096u);
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

} // anonymous namespace
} // namespace euxis::inference
