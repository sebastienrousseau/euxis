#include <gtest/gtest.h>
#include "euxis/runtime/agent_session.hpp"
#include <memory>

namespace euxis::runtime {
namespace {

class MockProvider : public IProvider {
public:
    std::string next_output;
    bool next_success{true};
    double next_duration{10.0};

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
                .error = {}, .duration_ms = next_duration};
    }

    auto execute(const ModelSpec& /*spec*/,
                 const std::string& /*prompt*/,
                 int /*timeout_ms*/,
                 const std::optional<std::vector<std::string>>& /*stop*/,
                 std::function<void(const std::string&)> /*stream*/) -> ProviderResponse override {
        return ProviderResponse{.success = next_success, .output = next_output,
                .error = {}, .duration_ms = next_duration};
    }
};

class AgentSessionTest : public ::testing::Test {
protected:
    MockProvider provider_;
    AgentSession session_{"s1", "a1", &provider_};
};

TEST_F(AgentSessionTest, SendRecordsHistory) {
    provider_.next_output = "Hello world";
    auto result = session_.send("Hi");

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.output, "Hello world");
    ASSERT_EQ(session_.messages().size(), 2u);
    EXPECT_EQ(session_.messages()[0].role, Role::User);
    EXPECT_EQ(session_.messages()[1].role, Role::Assistant);
}

TEST_F(AgentSessionTest, ToolRegistration) {
    session_.register_tool(
        {.name = "add", .description = "Add numbers", .input_schema = {}},
        [](const nlohmann::json& input) -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"result", input["a"].get<int>() + input["b"].get<int>()}};
        }
    );
}

TEST_F(AgentSessionTest, ModelSwitch) {
    ModelSpec spec{.provider = "mock", .model = "gpt-4", .tier = "reason",
                       .estimated_cost_per_1m = 0.0};
    EXPECT_TRUE(session_.switch_model(spec));
}

} // namespace
} // namespace euxis::runtime
