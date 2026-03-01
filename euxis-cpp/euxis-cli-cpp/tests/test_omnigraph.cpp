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

} // namespace
} // namespace euxis::cli
