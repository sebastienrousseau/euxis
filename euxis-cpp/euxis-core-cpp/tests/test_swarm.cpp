#include <gtest/gtest.h>

#include "euxis/core/swarm.hpp"

namespace euxis::core {
namespace {

TEST(SwarmOrchestratorTest, SimulationModeExecutes) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "test-playbook"},
        {"phases", {{
            {"name", "Phase 1"},
            {"mode", "sequential"},
            {"delegates", {{
                {"agent", "architect"},
                {"task_template", "Design ${goal}"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test system");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["agent"], "architect");
    EXPECT_TRUE(history[0]["result"].get<std::string>().find("Simulated") !=
                std::string::npos);
}

TEST(SwarmOrchestratorTest, EmptyPlaybook) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {{"name", "empty"}, {"phases", nlohmann::json::array()}};
    auto history = sw.execute_playbook(playbook, "nothing");
    EXPECT_TRUE(history.empty());
}

TEST(SwarmOrchestratorTest, GoalSubstitution) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "sub-test"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "coder"},
                {"task_template", "Build ${goal} now"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "widget");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["task"], "Build widget now");
}

TEST(SwarmOrchestratorTest, ParallelModeExecutesConcurrently) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "parallel-test"},
        {"phases", {{
            {"name", "Parallel Phase"},
            {"mode", "parallel"},
            {"delegates", {
                {{"agent", "coder"}, {"task_template", "Task A for ${goal}"}},
                {{"agent", "tester"}, {"task_template", "Task B for ${goal}"}},
                {{"agent", "reviewer"}, {"task_template", "Task C for ${goal}"}},
            }},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "feature");
    ASSERT_EQ(history.size(), 3);

    // All three agents should be present (order may vary in parallel)
    std::set<std::string> agents;
    for (const auto& h : history) {
        agents.insert(h["agent"].get<std::string>());
    }
    EXPECT_TRUE(agents.count("coder"));
    EXPECT_TRUE(agents.count("tester"));
    EXPECT_TRUE(agents.count("reviewer"));
}

TEST(SwarmOrchestratorTest, MixedSequentialAndParallelPhases) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "mixed"},
        {"phases", {
            {{"name", "P1"}, {"mode", "sequential"},
             {"delegates", {{{"agent", "planner"}, {"task_template", "Plan ${goal}"}}}}},
            {{"name", "P2"}, {"mode", "parallel"},
             {"delegates", {
                 {{"agent", "coder1"}, {"task_template", "Code A"}},
                 {{"agent", "coder2"}, {"task_template", "Code B"}},
             }}},
        }},
    };
    auto history = sw.execute_playbook(playbook, "app");
    EXPECT_EQ(history.size(), 3);
}

} // namespace
} // namespace euxis::core
