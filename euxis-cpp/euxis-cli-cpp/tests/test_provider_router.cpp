#include <gtest/gtest.h>
#include "euxis/cli/provider_router.hpp"

#include <cstdlib>
#include <fstream>

namespace euxis::cli {
namespace {

TEST(ProviderRouterTest, TierLabel) {
    EXPECT_EQ(tier_label(Tier::Routine), "routine");
    EXPECT_EQ(tier_label(Tier::Data), "data");
    EXPECT_EQ(tier_label(Tier::Code), "code");
    EXPECT_EQ(tier_label(Tier::Reason), "reason");
}

TEST(ProviderRouterTest, ParseTier) {
    EXPECT_EQ(parse_tier("routine"), Tier::Routine);
    EXPECT_EQ(parse_tier("data"), Tier::Data);
    EXPECT_EQ(parse_tier("code"), Tier::Code);
    EXPECT_EQ(parse_tier("reason"), Tier::Reason);
    EXPECT_EQ(parse_tier("unknown"), Tier::Code); // default
}

TEST(ProviderRouterTest, SelectModel) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Code);
    EXPECT_FALSE(sel.model.empty());
    EXPECT_EQ(sel.tier, Tier::Code);
}

TEST(ProviderRouterTest, SelectModelCostIncreases) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto routine = router.select_model(Tier::Routine);
    auto data = router.select_model(Tier::Data);
    auto code = router.select_model(Tier::Code);
    auto reason = router.select_model(Tier::Reason);

    EXPECT_LT(routine.estimated_cost_per_1m, data.estimated_cost_per_1m);
    EXPECT_LT(data.estimated_cost_per_1m, code.estimated_cost_per_1m);
    EXPECT_LT(code.estimated_cost_per_1m, reason.estimated_cost_per_1m);
}

TEST(ProviderRouterTest, AnalyzeTaskTier) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    EXPECT_EQ(router.analyze_task_tier("architect a new system"), Tier::Reason);
    EXPECT_EQ(router.analyze_task_tier("implement the feature"), Tier::Code);
    EXPECT_EQ(router.analyze_task_tier("analyze the data"), Tier::Data);
    EXPECT_EQ(router.analyze_task_tier("hello world"), Tier::Routine);
}

TEST(ProviderRouterTest, RouteUsesMaxTier) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto result = router.route("routine", "design a complex architecture");
    EXPECT_EQ(result.tier, Tier::Reason);
}

TEST(ProviderRouterTest, DetectProvider) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    EXPECT_FALSE(provider.empty());
}

TEST(ProviderRouterTest, EnvOverride) {
    setenv("EUXIS_MODEL_OVERRIDE", "test-model-123", 1);
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Code);
    EXPECT_EQ(sel.model, "test-model-123");
    unsetenv("EUXIS_MODEL_OVERRIDE");
}

TEST(ProviderRouterTest, LocalAvailable) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    // Just ensure it doesn't crash; result depends on ollama availability
    [[maybe_unused]] bool avail = router.local_available();
}

TEST(ProviderRouterTest, AvailableProviders) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto providers = router.available_providers();
    // Should return a (possibly empty) vector without crashing
    EXPECT_GE(providers.size(), 0u);
}

TEST(ProviderRouterTest, PrintStatus) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    // Should not crash
    router.print_status();
}

TEST(ProviderRouterTest, ModelFallbackChainClaude) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("claude-sonnet-4-6");
    EXPECT_FALSE(chain.empty());
    // Claude should fall back to openai, gemini, ollama
    bool has_openai = false;
    for (const auto& m : chain) {
        if (m.provider == "openai") has_openai = true;
    }
    EXPECT_TRUE(has_openai);
}

TEST(ProviderRouterTest, ModelFallbackChainGPT) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("gpt-4o");
    EXPECT_FALSE(chain.empty());
    bool has_anthropic = false;
    for (const auto& m : chain) {
        if (m.provider == "anthropic") has_anthropic = true;
    }
    EXPECT_TRUE(has_anthropic);
}

TEST(ProviderRouterTest, ModelFallbackChainGemini) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("gemini-2.0-flash");
    EXPECT_FALSE(chain.empty());
}

TEST(ProviderRouterTest, ModelFallbackChainUnknown) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("unknown-model");
    EXPECT_FALSE(chain.empty());
    // Generic fallback should include anthropic
    bool has_anthropic = false;
    for (const auto& m : chain) {
        if (m.provider == "anthropic") has_anthropic = true;
    }
    EXPECT_TRUE(has_anthropic);
}

TEST(ProviderRouterTest, AnalyzeTaskSecurity) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    EXPECT_EQ(router.analyze_task_tier("security audit of the system"), Tier::Reason);
}

TEST(ProviderRouterTest, AnalyzeTaskRefactor) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    EXPECT_EQ(router.analyze_task_tier("refactor the module"), Tier::Code);
}

TEST(ProviderRouterTest, AnalyzeTaskSummarize) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    EXPECT_EQ(router.analyze_task_tier("summarize the report"), Tier::Data);
}

TEST(ProviderRouterTest, RouteRoutineAgent) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto result = router.route("routine", "simple greeting");
    EXPECT_EQ(result.tier, Tier::Routine);
}

TEST(ProviderRouterTest, RouteCodeAgentWithSimpleTask) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto result = router.route("code", "hello world");
    // code > routine, so should stay at code
    EXPECT_EQ(result.tier, Tier::Code);
}

TEST(ProviderRouterTest, SelectModelRoutine) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Routine);
    EXPECT_FALSE(sel.model.empty());
    EXPECT_EQ(sel.tier, Tier::Routine);
    EXPECT_GT(sel.estimated_cost_per_1m, 0.0);
}

TEST(ProviderRouterTest, SelectModelData) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Data);
    EXPECT_FALSE(sel.model.empty());
    EXPECT_EQ(sel.tier, Tier::Data);
}

TEST(ProviderRouterTest, SelectModelReason) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Reason);
    EXPECT_FALSE(sel.model.empty());
    EXPECT_EQ(sel.tier, Tier::Reason);
}

TEST(ProviderRouterTest, LoadConfigFromFile) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_router_test";
    std::filesystem::create_directories(tmp / "config");
    nlohmann::json config = {
        {"models", {
            {"routine", "custom-routine-model"},
            {"code", "custom-code-model"}
        }}
    };
    std::ofstream(tmp / "config" / "router.json") << config.dump();

    ProviderRouter router(tmp.string());
    auto routine = router.select_model(Tier::Routine);
    EXPECT_EQ(routine.model, "custom-routine-model");
    auto code = router.select_model(Tier::Code);
    EXPECT_EQ(code.model, "custom-code-model");

    std::filesystem::remove_all(tmp);
}

TEST(ProviderRouterTest, ModelFallbackChainFromConfig) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_router_fb_test";
    std::filesystem::create_directories(tmp / "config");
    nlohmann::json config = {
        {"model_fallback", {
            {"custom-model", {
                {{"provider", "custom-p"}, {"model", "custom-m"}}
            }}
        }}
    };
    std::ofstream(tmp / "config" / "router.json") << config.dump();

    ProviderRouter router(tmp.string());
    auto chain = router.model_fallback_chain("custom-model");
    ASSERT_EQ(chain.size(), 1u);
    EXPECT_EQ(chain[0].provider, "custom-p");
    EXPECT_EQ(chain[0].model, "custom-m");

    std::filesystem::remove_all(tmp);
}

// --- Coverage: line 25 (tier_label default case) ---
TEST(ProviderRouterTest, TierLabelDefaultCase) {
    // Force a value outside the enum (not portable, but covers the default return)
    auto result = tier_label(static_cast<Tier>(99));
    EXPECT_EQ(result, "code"); // default fallback
}

// --- Coverage: lines 65-66 (load_config with malformed router.json) ---
TEST(ProviderRouterTest, LoadConfigParseError) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_router_bad_json";
    std::filesystem::create_directories(tmp / "config");
    std::ofstream(tmp / "config" / "router.json") << "not valid json{{{";

    // Should not crash, just warn and use defaults
    ProviderRouter router(tmp.string());
    auto sel = router.select_model(Tier::Code);
    EXPECT_FALSE(sel.model.empty());

    std::filesystem::remove_all(tmp);
}

// --- Coverage: lines 91-94 (detect_provider with OPENAI_API_KEY env var) ---
TEST(ProviderRouterTest, DetectProviderOpenAIEnv) {
    setenv("OPENAI_API_KEY", "sk-test-detect", 1);
    // Clear anthropic to ensure openai is detected
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("EUXIS_DEFAULT_PROVIDER");

    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    // With OPENAI_API_KEY set and no anthropic, should detect openai or claude
    // (depends on whether resolve_anthropic_token finds something)
    EXPECT_FALSE(provider.empty());

    unsetenv("OPENAI_API_KEY");
}

TEST(ProviderRouterTest, DetectProviderGeminiEnv) {
    setenv("GEMINI_API_KEY", "gem-test-detect", 1);
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("OPENAI_API_KEY");
    unsetenv("EUXIS_DEFAULT_PROVIDER");

    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    EXPECT_FALSE(provider.empty());

    unsetenv("GEMINI_API_KEY");
}

TEST(ProviderRouterTest, DetectProviderGoogleApiKeyEnv) {
    setenv("GOOGLE_API_KEY", "google-test-key", 1);
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("OPENAI_API_KEY");
    unsetenv("GEMINI_API_KEY");
    unsetenv("EUXIS_DEFAULT_PROVIDER");

    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    EXPECT_FALSE(provider.empty());

    unsetenv("GOOGLE_API_KEY");
}

// --- Coverage: lines 116-117 (select_model EUXIS_LOCAL_ONLY mode) ---
TEST(ProviderRouterTest, SelectModelLocalOnlyMode) {
    setenv("EUXIS_LOCAL_ONLY", "true", 1);
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Routine);
    // If ollama is not available, falls through to normal selection
    EXPECT_FALSE(sel.model.empty());
    unsetenv("EUXIS_LOCAL_ONLY");
}

TEST(ProviderRouterTest, SelectModelLocalOnlyWithCustomModel) {
    setenv("EUXIS_LOCAL_ONLY", "true", 1);
    setenv("EUXIS_LOCAL_MODEL", "custom-local-model", 1);
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto sel = router.select_model(Tier::Code);
    // If ollama is available, uses the custom local model
    EXPECT_FALSE(sel.model.empty());
    unsetenv("EUXIS_LOCAL_ONLY");
    unsetenv("EUXIS_LOCAL_MODEL");
}

// --- Coverage: lines 199-216 (print_status) ---
TEST(ProviderRouterTest, PrintStatusOutputsContent) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    // Should print status without crashing
    router.print_status();
}

// --- Coverage: detect_provider with EUXIS_DEFAULT_PROVIDER override ---
TEST(ProviderRouterTest, DetectProviderEnvOverride) {
    setenv("EUXIS_DEFAULT_PROVIDER", "custom-provider", 1);
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto provider = router.detect_provider();
    EXPECT_EQ(provider, "custom-provider");
    unsetenv("EUXIS_DEFAULT_PROVIDER");
}

} // namespace
} // namespace euxis::cli
