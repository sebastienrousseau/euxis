#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/runtime/agent_session.hpp"

namespace euxis::runtime {

// Forward declaration
auto make_session_store(const std::string& base_dir) -> std::unique_ptr<ISessionStore>;

namespace {

/// Mock provider for testing.
class MockProvider : public IProvider {
public:
    std::string next_output = "Mock response";
    double next_duration = 42.0;
    bool next_success = true;
    ModelSpec last_routed;
    ModelSpec last_executed;
    int execute_count{0};

    auto route(const std::string& agent_tier,
               const std::string&) -> ModelSpec override {
        last_routed = {.provider = "mock", .model = "mock-" + agent_tier,
                       .tier = agent_tier, .estimated_cost_per_1m = 0.0};
        return last_routed;
    }

    auto execute(const ModelSpec& spec,
                 const std::string&,
                 int) -> ProviderResult override {
        last_executed = spec;
        ++execute_count;
        return {.success = next_success, .output = next_output,
                .error = {}, .duration_ms = next_duration};
    }

    auto switch_model(const ModelSpec&) -> bool override { return true; }
};

class AgentSessionTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    MockProvider provider_;
    std::unique_ptr<ISessionStore> store_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_session_test";
        std::filesystem::remove_all(tmp_);
        store_ = make_session_store(tmp_.string());
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

TEST_F(AgentSessionTest, SendAndReceive) {
    AgentSession session("test-session", "agent-01", &provider_, store_.get());
    auto result = session.send("Hello");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "Mock response");
    EXPECT_EQ(provider_.execute_count, 1);
    EXPECT_EQ(session.messages().size(), 2u);  // user + assistant
    EXPECT_EQ(session.messages()[0].role, Role::User);
    EXPECT_EQ(session.messages()[1].role, Role::Assistant);
}

TEST_F(AgentSessionTest, SystemPromptIncluded) {
    AgentSession session("test-session", "agent-02", &provider_, store_.get());
    session.set_system_prompt("You are helpful.");
    session.send("Hi");

    EXPECT_EQ(provider_.execute_count, 1);
}

TEST_F(AgentSessionTest, ToolRegistration) {
    AgentSession session("test-session", "agent-03", &provider_, store_.get());

    session.register_tool(
        {.name = "add", .description = "Add two numbers", .input_schema = {}},
        [](const nlohmann::json& input) -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"result", input["a"].get<int>() + input["b"].get<int>()}};
        }
    );

    session.send("Use the add tool");
    EXPECT_EQ(provider_.execute_count, 1);
}

TEST_F(AgentSessionTest, SaveAndLoad) {
    {
        AgentSession session("persist-test", "agent-04", &provider_, store_.get());
        session.send("Message 1");
        session.send("Message 2");
        auto result = session.save();
        ASSERT_TRUE(result.has_value()) << result.error();
    }

    {
        AgentSession session("persist-test", "agent-04", &provider_, store_.get());
        auto result = session.load();
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(session.messages().size(), 4u);  // 2 user + 2 assistant
    }
}

TEST_F(AgentSessionTest, Branching) {
    AgentSession session("branch-test", "agent-05", &provider_, store_.get());
    session.send("Initial message");

    session.branch("experiment");
    EXPECT_EQ(session.current_branch(), "experiment");

    session.send("Branch message");
    auto result = session.save();
    ASSERT_TRUE(result.has_value());

    // Verify branch file exists
    auto branches = store_->list_branches("branch-test");
    EXPECT_GE(branches.size(), 1u);
}

TEST_F(AgentSessionTest, Compaction) {
    AgentSession session("compact-test", "agent-06", &provider_, store_.get());
    for (int i = 0; i < 10; ++i)
        session.send("Message " + std::to_string(i));

    EXPECT_EQ(session.messages().size(), 20u);  // 10 user + 10 assistant

    auto result = session.compact(4);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(session.messages().size(), 4u);
}

TEST_F(AgentSessionTest, EventEmission) {
    AgentSession session("event-test", "agent-07", &provider_, store_.get());

    int events_received = 0;
    session.on_event([&](SessionEvent, const std::string&) {
        ++events_received;
    });

    session.send("Hello");
    EXPECT_GE(events_received, 2);  // MessageSent + ResponseReceived
}

TEST_F(AgentSessionTest, ModelSwitch) {
    AgentSession session("switch-test", "agent-08", &provider_, store_.get());
    ModelSpec new_spec{.provider = "openai", .model = "gpt-4", .tier = "reason",
                       .estimated_cost_per_1m = 0.0};
    EXPECT_TRUE(session.switch_model(new_spec));
}

// --- Coverage: lines 58-60 (remove_tool) ---
TEST_F(AgentSessionTest, RemoveTool) {
    AgentSession session("rm-tool-test", "agent-rm", &provider_, store_.get());
    session.register_tool(
        {.name = "temp_tool", .description = "Temporary", .input_schema = {}},
        [](const nlohmann::json&) { return nlohmann::json{}; }
    );
    session.remove_tool("temp_tool");
    // After removal, send should still work (tool just won't be listed)
    auto result = session.send("test");
    EXPECT_TRUE(result.success);
}

// --- Coverage: lines 66-69 (append_system_context with multiple calls) ---
TEST_F(AgentSessionTest, AppendSystemContext) {
    AgentSession session("ctx-test", "agent-ctx", &provider_, store_.get());
    session.append_system_context("Context A");
    session.append_system_context("Context B");
    session.send("Hello");
    EXPECT_EQ(provider_.execute_count, 1);
}

// --- Coverage: lines 124-125 (session_id and agent_id getters) ---
TEST_F(AgentSessionTest, Getters) {
    AgentSession session("getter-test", "agent-get", &provider_, store_.get());
    EXPECT_EQ(session.session_id(), "getter-test");
    EXPECT_EQ(session.agent_id(), "agent-get");
    EXPECT_EQ(session.current_branch(), "main");
}

// --- Coverage: line 140 (save without store returns error) ---
TEST_F(AgentSessionTest, SaveWithoutStoreReturnsError) {
    AgentSession session("no-store", "agent-ns", &provider_, nullptr);
    auto result = session.save();
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("No session store"), std::string::npos);
}

// --- Coverage: line 156 (load without store returns error) ---
TEST_F(AgentSessionTest, LoadWithoutStoreReturnsError) {
    AgentSession session("no-store", "agent-ns", &provider_, nullptr);
    auto result = session.load();
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("No session store"), std::string::npos);
}

// --- Coverage: load nonexistent session returns error ---
TEST_F(AgentSessionTest, LoadNonexistentSessionFails) {
    AgentSession session("nonexistent-sess", "agent-ne", &provider_, store_.get());
    auto result = session.load("main");
    EXPECT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::runtime
