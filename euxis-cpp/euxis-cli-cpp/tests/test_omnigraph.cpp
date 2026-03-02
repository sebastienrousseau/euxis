#include <gtest/gtest.h>
#include "euxis/cli/omnigraph.hpp"

namespace euxis::cli {
namespace {

TEST(GraphAdapterTest, ClaudeFormat) {
    nlohmann::json graph = {
        {"nodes", {{"main.py", {{"type", "module"},
                                {"classes", {"App"}},
                                {"functions", {"run"}}}}}},
        {"edges", {{{"source", "main.py"},
                    {"relation", "imports"},
                    {"target", "utils.py"}}}},
    };
    auto result = GraphAdapter::format_for_provider(graph, "claude");
    EXPECT_TRUE(result.find("OMNIGRAPH") != std::string::npos);
    EXPECT_TRUE(result.find("main.py") != std::string::npos);
    EXPECT_TRUE(result.find("RELATIONSHIPS") != std::string::npos);
}

TEST(GraphAdapterTest, OpenAIFormat) {
    nlohmann::json graph = {{"nodes", {{"a.py", {{"type", "module"}}}}}};
    auto result = GraphAdapter::format_for_provider(graph, "openai");
    EXPECT_TRUE(result.find("{") != std::string::npos);
}

TEST(GraphAdapterTest, OllamaFormat) {
    nlohmann::json graph = {{"nodes", {{"a.py", {{"type", "module"}}}}}};
    auto result = GraphAdapter::format_for_provider(graph, "ollama");
    EXPECT_TRUE(result.find("Workspace Map") != std::string::npos);
}

TEST(TokenBudgeterTest, EstimateTokens) {
    TokenBudgeter b;
    EXPECT_EQ(b.estimate_tokens("1234"), 1);
    EXPECT_EQ(b.estimate_tokens("12345678"), 2);
}

TEST(TokenBudgeterTest, PrunesLargeGraph) {
    TokenBudgeter b(10);
    nlohmann::json graph = {
        {"nodes", {{"a", {}}, {"b", {}}}},
        {"edges", {{{"source", "a"}, {"target", "b"}, {"relation", "x"}}}},
    };
    auto pruned = b.optimize_graph(graph, "json");
    EXPECT_TRUE(pruned["edges"].empty());
}

// --- Coverage: line 20 (unknown provider falls back to system prompt format) ---
TEST(GraphAdapterTest, UnknownProviderDefaultsToSystemPrompt) {
    nlohmann::json graph = {
        {"nodes", {{"main.py", {{"type", "module"}}}}},
        {"edges", nlohmann::json::array()},
    };
    auto result = GraphAdapter::format_for_provider(graph, "unknown-provider");
    EXPECT_TRUE(result.find("OMNIGRAPH") != std::string::npos);
}

// --- Coverage: line 58 (system prompt format via format_for_provider("claude")) ---
TEST(GraphAdapterTest, SystemPromptViaClaudeProvider) {
    nlohmann::json graph = {
        {"nodes", {{"a", {{"label", "test"}}}}},
        {"edges", nlohmann::json::array()},
    };
    auto result = GraphAdapter::format_for_provider(graph, "claude");
    EXPECT_FALSE(result.empty());
}

// --- Coverage: line 71 (minimal format via format_for_provider("ollama")) ---
TEST(GraphAdapterTest, MinimalFormatViaOllamaProvider) {
    nlohmann::json graph = {{"nodes", nlohmann::json::object()}};
    auto result = GraphAdapter::format_for_provider(graph, "ollama");
    EXPECT_FALSE(result.empty());
}

// --- Coverage: line 87 (optimize_graph within budget does not prune) ---
TEST(TokenBudgeterTest, WithinBudgetNoPruning) {
    TokenBudgeter b(100000);
    nlohmann::json graph = {
        {"nodes", {{"a", {}}}},
        {"edges", {{{"source", "a"}, {"target", "b"}, {"relation", "x"}}}},
    };
    auto result = b.optimize_graph(graph, "json");
    EXPECT_FALSE(result["edges"].empty());
}

// --- Coverage: line 58 (format_system_prompt with populated graph) ---
TEST(GraphAdapterTest, SystemPromptFormatContent) {
    nlohmann::json graph = {
        {"nodes", {{"main.py", {{"type", "module"}, {"classes", {"App"}}}}}},
        {"edges", {
            {{"source", "main.py"}, {"relation", "imports"}, {"target", "utils.py"}}
        }},
    };
    auto result = GraphAdapter::format_for_provider(graph, "claude");
    EXPECT_NE(result.find("OMNIGRAPH"), std::string::npos);
    EXPECT_NE(result.find("RELATIONSHIPS"), std::string::npos);
    EXPECT_FALSE(result.empty());
}

// --- Coverage: line 71 (format_minimal with populated graph) ---
TEST(GraphAdapterTest, MinimalFormatContent) {
    nlohmann::json graph = {
        {"nodes", {{"a.py", {{"type", "module"}}}, {"b.py", {{"type", "module"}}}}},
    };
    auto result = GraphAdapter::format_for_provider(graph, "ollama");
    EXPECT_NE(result.find("Workspace Map"), std::string::npos);
    EXPECT_FALSE(result.empty());
}

// --- Coverage: line 87 (optimize_graph within budget returns full graph) ---
TEST(TokenBudgeterTest, WithinBudgetReturnsFull) {
    TokenBudgeter b(1000000); // very large budget
    nlohmann::json graph = {
        {"nodes", {{"a", {{"type", "file"}}}, {"b", {{"type", "file"}}}}},
        {"edges", {
            {{"source", "a"}, {"target", "b"}, {"relation", "imports"}}
        }},
    };
    auto result = b.optimize_graph(graph, "json");
    EXPECT_FALSE(result["nodes"].empty());
    EXPECT_FALSE(result["edges"].empty());
}

} // namespace
} // namespace euxis::cli
