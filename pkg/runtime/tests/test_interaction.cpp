#include <gtest/gtest.h>
#include "euxis/runtime/interaction.hpp"
#include <memory>

namespace euxis::runtime {
namespace {

class MockInteractionProvider : public IProvider {
public:
    std::string next_output;
    bool next_success{true};

    auto route(const std::string& /*tier*/, const std::string& agent_tier) -> ModelSpec override {
        return ModelSpec{.provider = "mock", .model = "mock-model",
                       .tier = agent_tier, .estimated_cost_per_1m = 0.0};
    }

    auto switch_model(const ModelSpec& /*spec*/) -> bool override { return true; }

    auto execute(const std::string& /*model*/,
                 const std::string& /*prompt*/,
                 int /*timeout_ms*/,
                 const std::optional<std::vector<std::string>>& /*stop*/,
                 std::function<void(const std::string&)> /*stream*/) -> ProviderResponse override {
        return ProviderResponse{.success = next_success, .output = next_output,
                .error = {}, .duration_ms = 10.0};
    }

    auto execute(const ModelSpec& /*spec*/,
                 const std::string& /*prompt*/,
                 int /*timeout_ms*/,
                 const std::optional<std::vector<std::string>>& /*stop*/,
                 std::function<void(const std::string&)> /*stream*/) -> ProviderResponse override {
        return ProviderResponse{.success = next_success, .output = next_output,
                .error = {}, .duration_ms = 10.0};
    }
};

class InteractionTest : public ::testing::Test {
protected:
    MockInteractionProvider provider_;
    // Store is null for these simple tests
    InteractionOrchestrator orchestrator_{&provider_, nullptr};
};

TEST_F(InteractionTest, SequentialFlow) {
    provider_.next_output = "Task complete";
    auto result = orchestrator_.run_sequential({"a1", "a2"}, "do something");

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.output, "Task complete");
    EXPECT_EQ(result.rounds, 2);
}

TEST_F(InteractionTest, EvaluatorLoop) {
    provider_.next_output = "0.8\nPerfect result";
    EvaluationGateConfig gate{
        .evaluator_agent_id = "eval",
        .evaluation_prompt = "rate it",
        .max_retries = 2,
        .min_quality_score = 0.7
    };

    auto result = orchestrator_.run_with_evaluator("worker", "do task", gate);
    ASSERT_TRUE(result.success);
}

TEST_F(InteractionTest, EvaluatorLoopFails) {
    provider_.next_output = "0.5\nNeeds work";
    EvaluationGateConfig gate{
        .evaluator_agent_id = "eval",
        .evaluation_prompt = "rate it",
        .max_retries = 1,
        .min_quality_score = 0.7
    };

    auto result = orchestrator_.run_with_evaluator("worker", "do task", gate);
    // Should fail because 0.5 < 0.7
    ASSERT_FALSE(result.success);
}

TEST_F(InteractionTest, SupervisorWorker) {
    provider_.next_output = "Subtask A\nSubtask B";
    SupervisorConfig config{
        .supervisor_agent_id = "boss",
        .worker_agent_ids = {"w1", "w2"},
        .decomposition_prompt = "break it down",
        .aggregation_prompt = "put it together",
        .parallel_workers = 2
    };

    auto result = orchestrator_.run_supervisor_worker(config, "big task");
    ASSERT_TRUE(result.success);
}

} // namespace
} // namespace euxis::runtime
