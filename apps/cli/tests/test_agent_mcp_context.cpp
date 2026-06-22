#include <gtest/gtest.h>
#include "euxis/cli/agent_mcp_context.hpp"

#include <thread>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::cli {

TEST(AgentMcpContextTest, PublishAndQuery) {
    AgentMcpContext ctx;
    ctx.publish_agent_findings("reviewer", "PASS", {"code looks good"}, 1500.0);

    auto result = ctx.query_agent("reviewer");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)["verdict"], "PASS");
}

TEST(AgentMcpContextTest, QueryUnknownAgent) {
    AgentMcpContext ctx;
    auto result = ctx.query_agent("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(AgentMcpContextTest, AvailableAgents) {
    AgentMcpContext ctx;
    ctx.publish_agent_findings("agent-a", "PASS", {}, 100.0);
    ctx.publish_agent_findings("agent-b", "FAIL", {}, 200.0);

    auto agents = ctx.available_agents();
    EXPECT_EQ(agents.size(), 2u);
    EXPECT_EQ(agents[0], "agent-a");
    EXPECT_EQ(agents[1], "agent-b");
}

TEST(AgentMcpContextTest, ContextSummary) {
    AgentMcpContext ctx;
    ctx.publish_agent_findings("librarian", "PASS", {"docs are current"}, 50.0);
    ctx.publish_agent_findings("sentinel", "FAIL", {"security issue found"}, 80.0);

    auto summary = ctx.build_context_summary();
    EXPECT_NE(summary.find("librarian"), std::string::npos);
    EXPECT_NE(summary.find("sentinel"), std::string::npos);
    EXPECT_NE(summary.find("security issue found"), std::string::npos);
}

TEST(AgentMcpContextTest, EmptySummary) {
    AgentMcpContext ctx;
    auto summary = ctx.build_context_summary();
    EXPECT_TRUE(summary.empty());
}

TEST(AgentMcpContextTest, HasAgent) {
    AgentMcpContext ctx;
    EXPECT_FALSE(ctx.has_agent("test"));
    ctx.publish_agent_findings("test", "PASS", {}, 10.0);
    EXPECT_TRUE(ctx.has_agent("test"));
}

TEST(AgentMcpContextTest, AgentCount) {
    AgentMcpContext ctx;
    EXPECT_EQ(ctx.agent_count(), 0u);
    ctx.publish_agent_findings("a", "PASS", {}, 10.0);
    EXPECT_EQ(ctx.agent_count(), 1u);
    ctx.publish_agent_findings("b", "FAIL", {}, 20.0);
    EXPECT_EQ(ctx.agent_count(), 2u);
}

TEST(AgentMcpContextTest, UpdateExistingAgent) {
    AgentMcpContext ctx;
    ctx.publish_agent_findings("agent", "TIMEOUT", {}, 100.0);
    ctx.publish_agent_findings("agent", "PASS", {"retried"}, 50.0);

    EXPECT_EQ(ctx.agent_count(), 1u);
    auto result = ctx.query_agent("agent");
    EXPECT_EQ((*result)["verdict"], "PASS");
}

TEST(AgentMcpContextTest, ThreadSafety) {
    AgentMcpContext ctx;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&ctx, i]() {
            ctx.publish_agent_findings("agent-" + std::to_string(i),
                "PASS", {"finding " + std::to_string(i)}, i * 10.0);
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(ctx.agent_count(), 10u);
}

TEST(AgentMcpContextTest, FindingsInSummary) {
    AgentMcpContext ctx;
    ctx.publish_agent_findings("reviewer", "PASS",
        {"clean code", "good tests", "well documented"}, 500.0);

    auto summary = ctx.build_context_summary();
    EXPECT_NE(summary.find("clean code"), std::string::npos);
    EXPECT_NE(summary.find("good tests"), std::string::npos);
}

} // namespace euxis::cli

// NOLINTEND(bugprone-unchecked-optional-access)
