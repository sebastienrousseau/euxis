#include <gtest/gtest.h>
#include "euxis/runtime/interaction.hpp"
#include <memory>

namespace euxis::runtime {
namespace {

class MockFullProvider : public IProvider {
public:
    std::string next_output{"success"};
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

TEST(InteractionExtendedTest, ScratchpadRoundtrip) {
    MockFullProvider provider;
    InteractionOrchestrator orchestrator(&provider, nullptr);
    Scratchpad scratch;
    
    auto result = orchestrator.run_scratchpad({"a1", "a2"}, "test task", scratch, 2);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.rounds, 2); 
    EXPECT_TRUE(scratch.read("output_a1").has_value());
    EXPECT_TRUE(scratch.read("output_a2").has_value());
}

TEST(InteractionExtendedTest, ScratchpadFailure) {
    MockFullProvider provider;
    provider.next_success = false;
    InteractionOrchestrator orchestrator(&provider, nullptr);
    Scratchpad scratch;
    
    auto result = orchestrator.run_scratchpad({"a1", "a2"}, "fail task", scratch, 1);
    EXPECT_FALSE(result.success);
}

TEST(InteractionExtendedTest, ParallelFlow) {
    MockFullProvider provider;
    InteractionOrchestrator orchestrator(&provider, nullptr);
    
    auto result = orchestrator.run_parallel({"a1", "a2", "a3"}, "parallel task");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.agent_outputs.size(), 3u);
}

TEST(InteractionExtendedTest, ScratchpadConcurrency) {
    Scratchpad s;
    s.write("key", "val");
    EXPECT_EQ(s.read("key").value(), "val");
    s.clear();
    EXPECT_FALSE(s.read("key").has_value());
}

TEST(InteractionExtendedTest, SequentialFailure) {
    MockFullProvider provider;
    provider.next_success = false;
    InteractionOrchestrator orchestrator(&provider, nullptr);
    
    auto result = orchestrator.run_sequential({"a1", "a2"}, "fail task");
    EXPECT_FALSE(result.success);
}

} // namespace
} // namespace euxis::runtime
