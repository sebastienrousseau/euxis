#include <gtest/gtest.h>

#include "euxis/inference/config.hpp"

namespace euxis::inference {
namespace {

// ---------------------------------------------------------------------------
// Roundtrip: to_json -> from_json preserves all fields
// ---------------------------------------------------------------------------
TEST(ConfigTest, RoundtripFullFields) {
    LocalModelConfig original;
    original.model_name      = "llama3-8b-q4";
    original.model_path      = "/models/llama3-8b-q4.gguf";
    original.context_size    = 4096;
    original.gpu_layers      = 32;
    original.threads         = 8;
    original.temperature     = 0.5f;
    original.top_p           = 0.95f;
    original.expected_sha256 = "abcdef1234567890";

    auto j         = original.to_json();
    auto recovered = LocalModelConfig::from_json(j);

    EXPECT_EQ(recovered.model_name, original.model_name);
    EXPECT_EQ(recovered.model_path, original.model_path);
    EXPECT_EQ(recovered.context_size, original.context_size);
    EXPECT_EQ(recovered.gpu_layers, original.gpu_layers);
    EXPECT_EQ(recovered.threads, original.threads);
    EXPECT_FLOAT_EQ(recovered.temperature, original.temperature);
    EXPECT_FLOAT_EQ(recovered.top_p, original.top_p);
    EXPECT_EQ(recovered.expected_sha256, original.expected_sha256);
}

// ---------------------------------------------------------------------------
// Default values when fields are missing
// ---------------------------------------------------------------------------
TEST(ConfigTest, DefaultValues) {
    nlohmann::json j = nlohmann::json::object();
    auto cfg         = LocalModelConfig::from_json(j);

    EXPECT_TRUE(cfg.model_name.empty());
    EXPECT_TRUE(cfg.model_path.empty());
    EXPECT_EQ(cfg.context_size, 2048u);
    EXPECT_EQ(cfg.gpu_layers, 0u);
    EXPECT_EQ(cfg.threads, 4u);
    EXPECT_FLOAT_EQ(cfg.temperature, 0.7f);
    EXPECT_FLOAT_EQ(cfg.top_p, 0.9f);
    EXPECT_TRUE(cfg.expected_sha256.empty());
}

// ---------------------------------------------------------------------------
// Partial JSON — only some fields provided
// ---------------------------------------------------------------------------
TEST(ConfigTest, PartialJson) {
    nlohmann::json j = {
        {"model_name",   "phi-3"},
        {"context_size", 1024},
    };
    auto cfg = LocalModelConfig::from_json(j);

    EXPECT_EQ(cfg.model_name, "phi-3");
    EXPECT_EQ(cfg.context_size, 1024u);
    // Other fields retain defaults
    EXPECT_EQ(cfg.gpu_layers, 0u);
    EXPECT_EQ(cfg.threads, 4u);
}

// ---------------------------------------------------------------------------
// to_json produces correct keys
// ---------------------------------------------------------------------------
TEST(ConfigTest, ToJsonKeys) {
    LocalModelConfig cfg;
    cfg.model_name = "test";
    auto j         = cfg.to_json();

    EXPECT_TRUE(j.contains("model_name"));
    EXPECT_TRUE(j.contains("model_path"));
    EXPECT_TRUE(j.contains("context_size"));
    EXPECT_TRUE(j.contains("gpu_layers"));
    EXPECT_TRUE(j.contains("threads"));
    EXPECT_TRUE(j.contains("temperature"));
    EXPECT_TRUE(j.contains("top_p"));
    EXPECT_TRUE(j.contains("expected_sha256"));
}

// ---------------------------------------------------------------------------
// from_json with extra keys — should not throw
// ---------------------------------------------------------------------------
TEST(ConfigTest, ExtraKeysIgnored) {
    nlohmann::json j = {
        {"model_name",   "test"},
        {"unknown_field", 42},
    };
    EXPECT_NO_THROW({
        auto cfg = LocalModelConfig::from_json(j);
        EXPECT_EQ(cfg.model_name, "test");
    });
}

} // anonymous namespace
} // namespace euxis::inference
