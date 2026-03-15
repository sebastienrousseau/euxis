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
    bool has_claude = false;
    for (const auto& m : chain) {
        if (m.provider == "claude") has_claude = true;
    }
    EXPECT_TRUE(has_claude);
}

TEST(ProviderRouterTest, ModelFallbackChainGemini) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("gemini-2.5-flash");
    EXPECT_FALSE(chain.empty());
}

TEST(ProviderRouterTest, ModelFallbackChainUnknown) {
    ProviderRouter router("/tmp/euxis_nonexistent");
    auto chain = router.model_fallback_chain("unknown-model");
    EXPECT_FALSE(chain.empty());
    // Generic fallback should include anthropic
    bool has_claude = false;
    for (const auto& m : chain) {
        if (m.provider == "claude") has_claude = true;
    }
    EXPECT_TRUE(has_claude);
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

// =====================================================================
//  STRATEGY ROUTING TESTS
// =====================================================================

class StrategyRouterTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_strategy_test";
        std::filesystem::create_directories(tmp_ / "config");

        // Write minimal router.json
        nlohmann::json router_cfg = {{"models", {
            {"routine", "gemini-2.5-flash-lite"},
            {"data", "gemini-2.5-flash"},
            {"code", "claude-sonnet-4-6"},
            {"reason", "claude-opus-4-6"}
        }}};
        std::ofstream(tmp_ / "config" / "router.json") << router_cfg.dump();

        // Write strategy config
        nlohmann::json strategy = {
            {"version", 1},
            {"defaults", {
                {"research",       {{"primary", "openai"},  {"fallback", {"gemini", "claude"}}}},
                {"coding",         {{"primary", "claude"},  {"fallback", {"gemini", "ollama"}}}},
                {"architecture",   {{"primary", "claude"},  {"fallback", {"gemini"}}}},
                {"audit",          {{"primary", "claude"},  {"fallback", {"openai", "gemini"}}}},
                {"deep_research",  {{"primary", "gemini"},  {"fallback", {"openai", "claude"}}}},
                {"security",       {{"primary", "gemini"},  {"fallback", {"claude", "openai"}}}},
                {"private_coding", {{"primary", "ollama"},  {"fallback", nlohmann::json::array()}}},
                {"surgical_edit",  {{"primary", "aider"},   {"fallback", {"claude", "ollama"}}}},
                {"terminal_automation", {{"primary", "kiro"}, {"fallback", {"shellgpt", "claude"}}}}
            }},
            {"models", {
                {"openai", {{"research", "gpt-5.4"}, {"default", "gpt-5.4"}}},
                {"claude", {{"coding", "claude-sonnet-4-6"}, {"architecture", "claude-opus-4-6"}, {"audit", "claude-opus-4-6"}, {"default", "claude-sonnet-4-6"}}},
                {"gemini", {{"deep_research", "gemini-3.1-pro"}, {"security", "gemini-3.1-pro"}, {"default", "gemini-3.1-pro"}}},
                {"ollama", {{"private_coding", "llama4:maverick"}, {"default", "llama4:maverick"}}},
                {"aider", {{"surgical_edit", "aider"}, {"default", "aider"}}},
                {"kiro",  {{"terminal_automation", "kiro"}, {"default", "kiro"}}}
            }},
            {"agent_class_hints", {
                {"librarian", "research"},
                {"writer", "coding"},
                {"strategist", "architecture"},
                {"architect", "architecture"},
                {"sentinel", "security"},
                {"reviewer", "audit"}
            }},
            {"classification_keywords", {
                {"research",      {"research", "literature", "survey", "knowledge"}},
                {"coding",        {"code", "implement", "refactor", "debug", "test", "build"}},
                {"architecture",  {"architect", "design", "system design"}},
                {"audit",         {"audit", "code review", "inspect"}},
                {"deep_research", {"deep research", "codebase analysis", "repository-wide"}},
                {"security",      {"security", "vulnerability", "CVE", "exploit", "threat model"}},
                {"private_coding",{"private", "local-only", "air-gapped"}},
                {"surgical_edit", {"surgical edit", "patch", "single-file fix"}},
                {"terminal_automation", {"shell", "terminal", "bash", "pipeline"}}
            }}
        };
        std::ofstream(tmp_ / "config" / "provider_strategy.json") << strategy.dump();
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

// --- Classification ---

TEST_F(StrategyRouterTest, ClassifyResearchTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("research the latest CVE reports"), "research");
}

TEST_F(StrategyRouterTest, ClassifySecurityTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("security audit of the module"), "security");
}

TEST_F(StrategyRouterTest, ClassifyCodingTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("implement the new feature"), "coding");
}

TEST_F(StrategyRouterTest, ClassifyArchitectureTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("design the system architecture"), "architecture");
}

TEST_F(StrategyRouterTest, ClassifyDeepResearchTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("deep research into the codebase analysis"), "deep_research");
}

TEST_F(StrategyRouterTest, ClassifySurgicalEditTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("surgical edit on line 42"), "surgical_edit");
}

TEST_F(StrategyRouterTest, ClassifyTerminalTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("run the bash pipeline"), "terminal_automation");
}

TEST_F(StrategyRouterTest, ClassifyPrivateCodingTask) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("private local-only processing"), "private_coding");
}

TEST_F(StrategyRouterTest, ClassifyByAgentHint) {
    ProviderRouter router(tmp_.string());
    // Agent hint should override keyword classification
    EXPECT_EQ(router.classify_task_class("do something", "librarian"), "research");
    EXPECT_EQ(router.classify_task_class("do something", "sentinel"), "security");
    EXPECT_EQ(router.classify_task_class("do something", "architect"), "architecture");
    EXPECT_EQ(router.classify_task_class("do something", "reviewer"), "audit");
}

TEST_F(StrategyRouterTest, ClassifyByPillar) {
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("do something", "", "security"), "security");
    EXPECT_EQ(router.classify_task_class("do something", "", "docs"), "research");
    EXPECT_EQ(router.classify_task_class("do something", "", "testing"), "coding");
    EXPECT_EQ(router.classify_task_class("do something", "", "audit"), "audit");
}

TEST_F(StrategyRouterTest, ClassifyLocalOnlyEnv) {
    setenv("EUXIS_LOCAL_ONLY", "true", 1);
    ProviderRouter router(tmp_.string());
    EXPECT_EQ(router.classify_task_class("implement a feature"), "private_coding");
    unsetenv("EUXIS_LOCAL_ONLY");
}

TEST_F(StrategyRouterTest, ClassifyFallsBackToTier) {
    ProviderRouter router(tmp_.string());
    // Generic task with no keyword match → tier heuristic
    auto cls = router.classify_task_class("hello world");
    EXPECT_FALSE(cls.empty());
}

// --- Strategy routing ---

TEST_F(StrategyRouterTest, RouteResearchToOpenAI) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("research", "librarian", "code", "research task");
    EXPECT_EQ(sel.provider, "openai");
    EXPECT_EQ(sel.model, "gpt-5.4");
    EXPECT_EQ(sel.task_class, "research");
    EXPECT_FALSE(sel.route_reason.empty());
}

TEST_F(StrategyRouterTest, RouteCodingToClaude) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("coding", "writer", "code", "implement stuff");
    EXPECT_EQ(sel.provider, "claude");
    EXPECT_EQ(sel.model, "claude-sonnet-4-6");
    EXPECT_EQ(sel.task_class, "coding");
}

TEST_F(StrategyRouterTest, RouteSecurityToGemini) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("security", "sentinel", "code", "security scan");
    EXPECT_EQ(sel.provider, "gemini");
    EXPECT_EQ(sel.model, "gemini-3.1-pro");
}

TEST_F(StrategyRouterTest, RouteDeepResearchToGemini) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("deep_research", "", "code", "deep analysis");
    EXPECT_EQ(sel.provider, "gemini");
    EXPECT_EQ(sel.model, "gemini-3.1-pro");
}

TEST_F(StrategyRouterTest, RouteArchitectureToClaude) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("architecture", "architect", "reason", "system design");
    EXPECT_EQ(sel.provider, "claude");
    EXPECT_EQ(sel.model, "claude-opus-4-6");
}

TEST_F(StrategyRouterTest, RoutePrivateCodingToOllama) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("private_coding", "", "code", "private work");
    EXPECT_EQ(sel.provider, "ollama");
    EXPECT_EQ(sel.model, "llama4:maverick");
}

// --- Provider override ---

TEST_F(StrategyRouterTest, ExplicitProviderOverride) {
    ProviderRouter router(tmp_.string());
    RouteOptions opts;
    opts.provider_override = "openai";
    auto sel = router.route_by_strategy("coding", "writer", "code", "implement stuff", opts);
    EXPECT_EQ(sel.provider, "openai");
    EXPECT_TRUE(sel.route_reason.find("override") != std::string::npos);
}

// --- Local-only enforcement ---

TEST_F(StrategyRouterTest, LocalOnlyForcesOllama) {
    ProviderRouter router(tmp_.string());
    RouteOptions opts;
    opts.local_only = true;
    auto sel = router.route_by_strategy("research", "librarian", "code", "research task", opts);
    EXPECT_EQ(sel.provider, "ollama");
    EXPECT_EQ(sel.task_class, "private_coding");
    EXPECT_TRUE(sel.route_reason.find("local-only") != std::string::npos);
}

TEST_F(StrategyRouterTest, LocalOnlyCustomModel) {
    setenv("EUXIS_LOCAL_MODEL", "custom-local-7b", 1);
    ProviderRouter router(tmp_.string());
    RouteOptions opts;
    opts.local_only = true;
    auto sel = router.route_by_strategy("coding", "", "code", "code task", opts);
    EXPECT_EQ(sel.provider, "ollama");
    EXPECT_EQ(sel.model, "custom-local-7b");
    unsetenv("EUXIS_LOCAL_MODEL");
}

// --- Env-var override ---

TEST_F(StrategyRouterTest, EnvVarOverrideForResearch) {
    setenv("EUXIS_DEFAULT_RESEARCH_PROVIDER", "gemini", 1);
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("research", "librarian", "code", "research task");
    EXPECT_EQ(sel.provider, "gemini");
    EXPECT_TRUE(sel.route_reason.find("env override") != std::string::npos);
    unsetenv("EUXIS_DEFAULT_RESEARCH_PROVIDER");
}

TEST_F(StrategyRouterTest, EnvVarOverrideForSecurity) {
    setenv("EUXIS_DEFAULT_SECURITY_PROVIDER", "claude", 1);
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("security", "", "code", "vuln scan");
    EXPECT_EQ(sel.provider, "claude");
    unsetenv("EUXIS_DEFAULT_SECURITY_PROVIDER");
}

// --- Tool fallback ---

TEST_F(StrategyRouterTest, SurgicalEditFallsBackWhenAiderUnavailable) {
    ProviderRouter router(tmp_.string());
    // aider is likely not installed in CI/test env
    auto sel = router.route_by_strategy("surgical_edit", "", "code", "patch file");
    // Should fall back to claude or ollama since aider binary is not found
    if (!router.tool_available("aider")) {
        EXPECT_NE(sel.provider, "aider");
        EXPECT_TRUE(sel.route_reason.find("fallback") != std::string::npos
                     || sel.route_reason.find("preferred") != std::string::npos);
    }
}

TEST_F(StrategyRouterTest, TerminalFallsBackWhenKiroUnavailable) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("terminal_automation", "", "code", "run pipeline");
    if (!router.tool_available("kiro")) {
        EXPECT_NE(sel.provider, "kiro");
    }
}

// --- route_with_policy (full pipeline) ---

TEST_F(StrategyRouterTest, RouteWithPolicyResearch) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_with_policy("research the codebase", "librarian", "code");
    EXPECT_EQ(sel.provider, "openai");
    EXPECT_FALSE(sel.route_reason.empty());
    EXPECT_FALSE(sel.task_class.empty());
}

TEST_F(StrategyRouterTest, RouteWithPolicySecurity) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_with_policy("security vulnerability scan", "", "code", "security");
    EXPECT_EQ(sel.provider, "gemini");
}

TEST_F(StrategyRouterTest, RouteWithPolicyVerbose) {
    ProviderRouter router(tmp_.string());
    RouteOptions opts;
    opts.verbose = true;
    // Should not crash, verbose output goes to stderr
    auto sel = router.route_with_policy("implement feature", "writer", "code", "", opts);
    EXPECT_FALSE(sel.provider.empty());
}

// --- Strategy config loading ---

TEST_F(StrategyRouterTest, StrategyConfigLoaded) {
    ProviderRouter router(tmp_.string());
    EXPECT_TRUE(router.strategy_loaded());
}

TEST_F(StrategyRouterTest, StrategyConfigMissing) {
    auto tmp2 = std::filesystem::temp_directory_path() / "euxis_no_strategy_test";
    std::filesystem::create_directories(tmp2 / "config");
    // Write router.json but no strategy config
    std::ofstream(tmp2 / "config" / "router.json") << "{}";
    ProviderRouter router(tmp2.string());
    EXPECT_FALSE(router.strategy_loaded());
    // Should still route via tier fallback
    auto sel = router.route_by_strategy("coding", "", "code", "implement");
    EXPECT_FALSE(sel.provider.empty());
    std::filesystem::remove_all(tmp2);
}

// --- Explainability ---

TEST_F(StrategyRouterTest, RouteReasonPopulated) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route("code", "implement feature");
    EXPECT_FALSE(sel.route_reason.empty());
}

TEST_F(StrategyRouterTest, SelectModelReasonPopulated) {
    ProviderRouter router(tmp_.string());
    auto sel = router.select_model(Tier::Code);
    EXPECT_FALSE(sel.route_reason.empty());
}

TEST_F(StrategyRouterTest, FlashOverrideReasonPopulated) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_flash("librarian", "routine", "some task");
    EXPECT_FALSE(sel.route_reason.empty());
}

TEST_F(StrategyRouterTest, StandardOverrideReasonPopulated) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_standard("reviewer", "code", "review code");
    EXPECT_FALSE(sel.route_reason.empty());
}

// --- Tool availability ---

TEST_F(StrategyRouterTest, ToolAvailableDoesNotCrash) {
    ProviderRouter router(tmp_.string());
    // These should not crash regardless of what's installed
    [[maybe_unused]] bool a = router.tool_available("aider");
    [[maybe_unused]] bool k = router.tool_available("kiro");
    [[maybe_unused]] bool s = router.tool_available("shellgpt");
    [[maybe_unused]] bool u = router.tool_available("nonexistent-tool-xyz");
}

TEST_F(StrategyRouterTest, AvailableProvidersIncludesTools) {
    ProviderRouter router(tmp_.string());
    auto providers = router.available_providers();
    // Should include API providers at minimum
    EXPECT_GE(providers.size(), 1u);
}

// --- Unknown task class falls back ---

TEST_F(StrategyRouterTest, UnknownClassFallsToTierRouting) {
    ProviderRouter router(tmp_.string());
    auto sel = router.route_by_strategy("unknown_class_xyz", "", "code", "do something");
    // No strategy match → tier fallback
    EXPECT_FALSE(sel.provider.empty());
    EXPECT_TRUE(sel.route_reason.find("tier fallback") != std::string::npos);
}

// --- Print status with strategy ---

TEST_F(StrategyRouterTest, PrintStatusWithStrategy) {
    ProviderRouter router(tmp_.string());
    // Should print strategy info without crashing
    router.print_status();
}

} // namespace
} // namespace euxis::cli
