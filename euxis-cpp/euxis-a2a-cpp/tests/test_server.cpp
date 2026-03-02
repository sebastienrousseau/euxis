#include <gtest/gtest.h>
#include <sodium.h>

#include <string>

#include "euxis/a2a/server.hpp"

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// Fixture: create a server handler with a valid card
// ---------------------------------------------------------------------------
class ServerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    static auto make_card() -> AgentCard {
        AgentCard card;
        card.name = "test-server-agent";
        card.description = "Agent for server handler tests";
        card.url = "http://localhost:9090";
        card.version = "0.2.0";

        Capability cap;
        cap.name = "echo";
        cap.description = "Echoes input";
        card.capabilities.push_back(std::move(cap));

        Capability cap2;
        cap2.name = "summarize";
        cap2.description = "Summarizes text";
        card.capabilities.push_back(std::move(cap2));

        return card;
    }

    A2AServerHandler handler_{make_card()};
};

// ---------------------------------------------------------------------------
// agent/card returns the agent's card
// ---------------------------------------------------------------------------
TEST_F(ServerTest, AgentCardMethod) {
    auto resp = handler_.handle_request("agent/card", {});

    ASSERT_TRUE(resp.contains("result"));
    const auto& result = resp["result"];
    EXPECT_EQ(result["name"].get<std::string>(), "test-server-agent");
    EXPECT_EQ(result["url"].get<std::string>(), "http://localhost:9090");
    EXPECT_EQ(result["version"].get<std::string>(), "0.2.0");
}

// ---------------------------------------------------------------------------
// capabilities/list returns capabilities
// ---------------------------------------------------------------------------
TEST_F(ServerTest, CapabilitiesListMethod) {
    auto resp = handler_.handle_request("capabilities/list", {});

    ASSERT_TRUE(resp.contains("result"));
    const auto& result = resp["result"];
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]["name"].get<std::string>(), "echo");
    EXPECT_EQ(result[1]["name"].get<std::string>(), "summarize");
}

// ---------------------------------------------------------------------------
// task/create creates a new task
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskCreate) {
    auto resp = handler_.handle_request("task/create", {});

    ASSERT_TRUE(resp.contains("result"));
    const auto& result = resp["result"];
    EXPECT_TRUE(result.contains("id"));
    EXPECT_EQ(result["status"].get<std::string>(), "pending");
    EXPECT_TRUE(result["messages"].empty());
}

// ---------------------------------------------------------------------------
// task/create with initial message
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskCreateWithMessage) {
    nlohmann::json params = {{"message", "Do something useful"}};
    auto resp = handler_.handle_request("task/create", params);

    ASSERT_TRUE(resp.contains("result"));
    const auto& result = resp["result"];
    ASSERT_EQ(result["messages"].size(), 1u);
    EXPECT_EQ(result["messages"][0]["role"].get<std::string>(), "user");
    EXPECT_EQ(result["messages"][0]["parts"][0]["content"].get<std::string>(),
              "Do something useful");
}

// ---------------------------------------------------------------------------
// task/get retrieves an existing task
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskGet) {
    // Create a task first
    auto create_resp = handler_.handle_request("task/create",
        {{"message", "lookup task"}});
    ASSERT_TRUE(create_resp.contains("result"));
    const auto task_id = create_resp["result"]["id"].get<std::string>();

    // Retrieve it
    auto get_resp = handler_.handle_request("task/get", {{"id", task_id}});
    ASSERT_TRUE(get_resp.contains("result"));
    EXPECT_EQ(get_resp["result"]["id"].get<std::string>(), task_id);
    EXPECT_EQ(get_resp["result"]["status"].get<std::string>(), "pending");
}

// ---------------------------------------------------------------------------
// task/get with unknown ID returns error
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskGetUnknownId) {
    auto resp = handler_.handle_request("task/get",
        {{"id", "nonexistent-id-12345"}});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_TRUE(resp["error"].contains("message"));
}

// ---------------------------------------------------------------------------
// task/get without ID parameter returns error
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskGetMissingId) {
    auto resp = handler_.handle_request("task/get", {});
    ASSERT_TRUE(resp.contains("error"));
}

// ---------------------------------------------------------------------------
// task/cancel transitions task to Cancelled
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskCancel) {
    // Create a task
    auto create_resp = handler_.handle_request("task/create", {});
    const auto task_id = create_resp["result"]["id"].get<std::string>();

    // Cancel it
    auto cancel_resp = handler_.handle_request("task/cancel",
        {{"id", task_id}});
    ASSERT_TRUE(cancel_resp.contains("result"));
    EXPECT_EQ(cancel_resp["result"]["status"].get<std::string>(), "cancelled");
    ASSERT_EQ(cancel_resp["result"]["history"].size(), 1u);
    EXPECT_EQ(cancel_resp["result"]["history"][0].get<std::string>(), "pending");
}

// ---------------------------------------------------------------------------
// task/cancel on already-cancelled task returns error
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskCancelAlreadyCancelled) {
    auto create_resp = handler_.handle_request("task/create", {});
    const auto task_id = create_resp["result"]["id"].get<std::string>();

    // Cancel once (succeeds)
    auto cancel1 = handler_.handle_request("task/cancel", {{"id", task_id}});
    ASSERT_TRUE(cancel1.contains("result"));

    // Cancel again (fails — terminal state)
    auto cancel2 = handler_.handle_request("task/cancel", {{"id", task_id}});
    ASSERT_TRUE(cancel2.contains("error"));
}

// ---------------------------------------------------------------------------
// task/cancel with unknown ID returns error
// ---------------------------------------------------------------------------
TEST_F(ServerTest, TaskCancelUnknownId) {
    auto resp = handler_.handle_request("task/cancel",
        {{"id", "does-not-exist"}});
    ASSERT_TRUE(resp.contains("error"));
}

// ---------------------------------------------------------------------------
// Unknown method returns error
// ---------------------------------------------------------------------------
TEST_F(ServerTest, UnknownMethodReturnsError) {
    auto resp = handler_.handle_request("invalid/method", {});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_TRUE(resp["error"].contains("message"));
    EXPECT_TRUE(resp["error"].contains("code"));
}

// ---------------------------------------------------------------------------
// Full task lifecycle: create -> get -> cancel -> get
// ---------------------------------------------------------------------------
TEST_F(ServerTest, FullTaskLifecycle) {
    // Create
    auto create_resp = handler_.handle_request("task/create",
        {{"message", "lifecycle test"}});
    ASSERT_TRUE(create_resp.contains("result"));
    const auto task_id = create_resp["result"]["id"].get<std::string>();
    EXPECT_EQ(create_resp["result"]["status"].get<std::string>(), "pending");

    // Get (still pending)
    auto get1 = handler_.handle_request("task/get", {{"id", task_id}});
    EXPECT_EQ(get1["result"]["status"].get<std::string>(), "pending");

    // Cancel
    auto cancel_resp = handler_.handle_request("task/cancel",
        {{"id", task_id}});
    EXPECT_EQ(cancel_resp["result"]["status"].get<std::string>(), "cancelled");

    // Get (now cancelled)
    auto get2 = handler_.handle_request("task/get", {{"id", task_id}});
    EXPECT_EQ(get2["result"]["status"].get<std::string>(), "cancelled");
}

// ---------------------------------------------------------------------------
// Multiple tasks are independent
// ---------------------------------------------------------------------------
TEST_F(ServerTest, MultipleTasks) {
    auto r1 = handler_.handle_request("task/create",
        {{"message", "task 1"}});
    auto r2 = handler_.handle_request("task/create",
        {{"message", "task 2"}});

    const auto id1 = r1["result"]["id"].get<std::string>();
    const auto id2 = r2["result"]["id"].get<std::string>();
    EXPECT_NE(id1, id2);

    // Cancel only task 1
    (void)handler_.handle_request("task/cancel", {{"id", id1}});

    // Task 1 is cancelled
    auto get1 = handler_.handle_request("task/get", {{"id", id1}});
    EXPECT_EQ(get1["result"]["status"].get<std::string>(), "cancelled");

    // Task 2 is still pending
    auto get2 = handler_.handle_request("task/get", {{"id", id2}});
    EXPECT_EQ(get2["result"]["status"].get<std::string>(), "pending");
}

// --- Coverage: line 43 (unknown method error response details) ---
TEST_F(ServerTest, UnknownMethodErrorHasCorrectCode) {
    auto resp = handler_.handle_request("bogus/method", {});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"].get<int>(), -32601);
    auto msg = resp["error"]["message"].get<std::string>();
    EXPECT_NE(msg.find("bogus/method"), std::string::npos);
}

// --- Coverage: line 84 (task/get with non-string id param) ---
TEST_F(ServerTest, TaskGetIdNotString) {
    auto resp = handler_.handle_request("task/get", {{"id", 12345}});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"].get<int>(), -32602);
}

// --- Coverage: line 95 (task/get with unknown string id) ---
TEST_F(ServerTest, TaskGetUnknownStringId) {
    auto resp = handler_.handle_request("task/get",
        {{"id", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"}});
    ASSERT_TRUE(resp.contains("error"));
    auto msg = resp["error"]["message"].get<std::string>();
    EXPECT_NE(msg.find("not found"), std::string::npos);
}

// --- Coverage: line 113 (task/cancel with non-string id) ---
TEST_F(ServerTest, TaskCancelIdNotString) {
    auto resp = handler_.handle_request("task/cancel", {{"id", 999}});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"].get<int>(), -32602);
}

// --- Coverage: line 116 (task/cancel without id) ---
TEST_F(ServerTest, TaskCancelMissingId) {
    auto resp = handler_.handle_request("task/cancel", {});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"].get<int>(), -32602);
}

// --- Coverage: line 124 (task/cancel on unknown id) ---
TEST_F(ServerTest, TaskCancelUnknownStringId) {
    auto resp = handler_.handle_request("task/cancel",
        {{"id", "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"}});
    ASSERT_TRUE(resp.contains("error"));
    auto msg = resp["error"]["message"].get<std::string>();
    EXPECT_NE(msg.find("not found"), std::string::npos);
}

// --- Coverage: line 134 (task/cancel transition failure error) ---
TEST_F(ServerTest, TaskCancelTransitionFailureReturnsError) {
    // Create and cancel a task (puts it in terminal state)
    auto create_resp = handler_.handle_request("task/create", {});
    auto id = create_resp["result"]["id"].get<std::string>();
    (void)handler_.handle_request("task/cancel", {{"id", id}});

    // Try to cancel again (transition should fail)
    auto resp = handler_.handle_request("task/cancel", {{"id", id}});
    ASSERT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"].get<int>(), -32603);
}

// --- Coverage: lines 154, 157 (capabilities with input/output schemas) ---
TEST_F(ServerTest, CapabilitiesWithSchemas) {
    AgentCard card;
    card.name = "schema-agent";
    card.description = "Agent with schemas";
    card.url = "http://localhost:8080";
    card.version = "1.0.0";

    Capability cap;
    cap.name = "typed-echo";
    cap.description = "Echo with schemas";
    cap.input_schema = nlohmann::json{{"type", "object"},
                                       {"properties", {{"text", {{"type", "string"}}}}}};
    cap.output_schema = nlohmann::json{{"type", "object"},
                                        {"properties", {{"echo", {{"type", "string"}}}}}};
    card.capabilities.push_back(std::move(cap));

    A2AServerHandler handler(std::move(card));
    auto resp = handler.handle_request("capabilities/list", {});
    ASSERT_TRUE(resp.contains("result"));
    ASSERT_EQ(resp["result"].size(), 1u);
    EXPECT_TRUE(resp["result"][0].contains("inputSchema"));
    EXPECT_TRUE(resp["result"][0].contains("outputSchema"));
    EXPECT_EQ(resp["result"][0]["inputSchema"]["type"], "object");
}

} // namespace
} // namespace euxis::a2a
