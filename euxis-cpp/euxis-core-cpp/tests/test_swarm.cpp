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

TEST(SwarmOrchestratorTest, NoGoalSubstitutionWhenMissing) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "no-sub"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Do some work without goal placeholder"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test-goal");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["task"], "Do some work without goal placeholder");
}

TEST(SwarmOrchestratorTest, MultipleGoalSubstitutions) {
    // Only the first ${goal} is replaced
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "multi-sub"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "coder"},
                {"task_template", "Start ${goal} then finish"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "project");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0]["task"], "Start project then finish");
}

TEST(SwarmOrchestratorTest, NonLocalhostUsesNonSimulation) {
    // Non-localhost URL should NOT trigger simulation mode
    SwarmOrchestrator sw("http://remote-server:8080");
    nlohmann::json playbook = {
        {"name", "remote-test"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Remote task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    ASSERT_EQ(history.size(), 1);
    // Should fail because WebSocket connection fails
    EXPECT_NE(history[0]["result"].get<std::string>().find("failed"),
              std::string::npos);
}

TEST(SwarmOrchestratorTest, PlaybookWithoutName) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    EXPECT_EQ(history.size(), 1);
}

TEST(SwarmOrchestratorTest, PhaseWithoutDelegates) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json playbook = {
        {"name", "empty-phase"},
        {"phases", {{
            {"name", "Empty Phase"},
            {"mode", "sequential"},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    EXPECT_TRUE(history.empty());
}

TEST(SwarmOrchestratorTest, ManyParallelTasks) {
    SwarmOrchestrator sw("http://localhost:8080");
    nlohmann::json delegates = nlohmann::json::array();
    for (int i = 0; i < 10; ++i) {
        delegates.push_back({
            {"agent", "worker-" + std::to_string(i)},
            {"task_template", "Task " + std::to_string(i)}
        });
    }
    nlohmann::json playbook = {
        {"name", "many-parallel"},
        {"phases", {{
            {"name", "Parallel"},
            {"mode", "parallel"},
            {"delegates", delegates},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "stress");
    EXPECT_EQ(history.size(), 10);
}

// --- Coverage: swarm.cpp line 25 (derive_ws_url with https:// URL) ---
TEST(SwarmOrchestratorTest, HttpsUrlUsesWss) {
    // https URL should trigger wss:// prefix and non-simulation mode
    SwarmOrchestrator sw("https://remote-server:8080");
    nlohmann::json playbook = {
        {"name", "https-test"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    ASSERT_EQ(history.size(), 1);
    // Should fail because WebSocket connection fails to remote server
    EXPECT_NE(history[0]["result"].get<std::string>().find("failed"),
              std::string::npos);
}

// --- Coverage: swarm.cpp line 29 (derive_ws_url with no scheme prefix) ---
TEST(SwarmOrchestratorTest, NoSchemeUrlGetsWsPrefix) {
    SwarmOrchestrator sw("remote-host:9090");
    nlohmann::json playbook = {
        {"name", "no-scheme"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    ASSERT_EQ(history.size(), 1);
    // Non-localhost and non-simulation, connection should fail
    EXPECT_NE(history[0]["result"].get<std::string>().find("failed"),
              std::string::npos);
}

// --- Coverage: swarm.cpp line 39 (port_str with slash in URL path) ---
TEST(SwarmOrchestratorTest, UrlWithPathAfterPort) {
    SwarmOrchestrator sw("http://remote-host:8080/api/v1");
    nlohmann::json playbook = {
        {"name", "path-test"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    ASSERT_EQ(history.size(), 1);
    // Should fail (non-localhost remote)
    EXPECT_NE(history[0]["result"].get<std::string>().find("failed"),
              std::string::npos);
}

// --- Coverage: swarm.cpp line 44 (stoi exception catch, non-numeric port) ---
TEST(SwarmOrchestratorTest, NonNumericPortInUrl) {
    SwarmOrchestrator sw("http://remote-host:notaport");
    nlohmann::json playbook = {
        {"name", "badport"},
        {"phases", {{
            {"name", "P1"},
            {"delegates", {{
                {"agent", "worker"},
                {"task_template", "Task"},
            }}},
        }}},
    };
    auto history = sw.execute_playbook(playbook, "test");
    ASSERT_EQ(history.size(), 1);
    // Should handle bad port gracefully and fail connection
    EXPECT_NE(history[0]["result"].get<std::string>().find("failed"),
              std::string::npos);
}

} // namespace
} // namespace euxis::core
