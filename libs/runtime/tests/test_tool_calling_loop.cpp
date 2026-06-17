/// @file
/// @brief Unit tests for the closed tool-calling loop pattern shown in
///        docs/examples/cpp/tool_calling_loop/. The example's
///        dispatch_tool_calls helper is reproduced inline here so the
///        runtime library has no dependency on docs/ — the contract we
///        guard is "given a JSON tool_calls array, the loop classifies,
///        gates, dispatches, and appends Tool-role messages".

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/agent_session.hpp"
#include "euxis/runtime/tool_manifest.hpp"

namespace euxis::runtime {
namespace {

// Local copy of the example's LocalToolRegistry so the test exercises
// the same contract.
class LocalRegistry final : public IToolRegistry {
public:
    void register_tool(ToolDeclaration decl, ToolHandler h) override {
        std::string n = decl.name;
        m_[std::move(n)] = {std::move(decl), std::move(h)};
    }
    auto invoke(const std::string& n, const nlohmann::json& in)
        -> std::expected<nlohmann::json, std::string> override {
        if (auto it = m_.find(n); it != m_.end()) return it->second.h(in);
        return std::unexpected("missing: " + n);
    }
    auto list_tools() const -> std::vector<ToolDeclaration> override {
        std::vector<ToolDeclaration> out;
        for (const auto& [_, e] : m_) out.push_back(e.decl);
        return out;
    }
    void remove_tool(const std::string& n) override { m_.erase(n); }
private:
    struct E { ToolDeclaration decl; ToolHandler h; };
    std::unordered_map<std::string, E> m_;
};

using ApprovalCallback =
    std::function<bool(ApprovalClass, std::string_view)>;

struct DispatchOutcome {
    std::size_t dispatched{0};
    std::size_t denied{0};
    std::size_t errored{0};
};

auto dispatch_tool_calls(const nlohmann::json& provider_output,
                         IToolRegistry& registry,
                         AgentLoopHarness& harness,
                         const ApprovalCallback& approve) -> DispatchOutcome {
    DispatchOutcome out;
    if (!provider_output.contains("tool_calls")) return out;
    for (const auto& call : provider_output.at("tool_calls")) {
        const auto name = call.value("name", std::string{});
        const auto args = call.value("args", nlohmann::json::object());
        ToolDeclaration_v2 d; d.name = name;
        const auto klass = classify_approval(d);

        SessionMessage tm{}; tm.role = Role::Assistant; tm.agent_id = harness.agent_id();
        if (!approve(klass, name)) {
            ++out.denied;
            tm.content = nlohmann::json{{"tool",name},{"status","denied"}}.dump();
            harness.add_message(ConversationMessage{tm});
            continue;
        }
        auto r = registry.invoke(name, args);
        if (r.has_value()) {
            ++out.dispatched;
            tm.content = nlohmann::json{
                {"tool",name},{"status","ok"},{"result",*r}}.dump();
        } else {
            ++out.errored;
            tm.content = nlohmann::json{
                {"tool",name},{"status","error"},{"error",r.error()}}.dump();
        }
        harness.add_message(ConversationMessage{tm});
    }
    return out;
}

auto build_registry() -> LocalRegistry {
    LocalRegistry r;
    r.register_tool({.name="list_files",.description="",.input_schema={}},
        [](const nlohmann::json& a) -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"path", a.value("path","")},{"entries", {"a"}}};
        });
    r.register_tool({.name="write_file",.description="",.input_schema={}},
        [](const nlohmann::json& a) -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"path", a.value("path","")},{"bytes", 0}};
        });
    r.register_tool({.name="set_strict_mode",.description="",.input_schema={}},
        [](const nlohmann::json& a) -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"enabled", a.value("enabled", false)}};
        });
    return r;
}

auto make_harness() -> AgentLoopHarness {
    AgentLoopHarness::Config cfg;
    cfg.session_id="t"; cfg.agent_id="a";
    cfg.iteration_budget_max=10; cfg.context_window=16'384;
    return AgentLoopHarness{std::move(cfg)};
}

// ---------------------------------------------------------------------------

TEST(ToolCallingLoop, ReadonlyCallAutoDispatches) {
    auto reg = build_registry();
    auto h = make_harness();
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(
            R"({"tool_calls":[{"name":"list_files","args":{"path":"src/"}}]})"),
        reg, h,
        [](ApprovalClass k, std::string_view) { return k == ApprovalClass::Readonly; });
    EXPECT_EQ(out.dispatched, 1u);
    EXPECT_EQ(out.denied, 0u);
    EXPECT_EQ(out.errored, 0u);
    ASSERT_EQ(h.messages().size(), 1u);
    EXPECT_EQ(h.messages().front().role, Role::Assistant);
    auto parsed = nlohmann::json::parse(h.messages().front().content);
    EXPECT_EQ(parsed["status"].get<std::string>(), "ok");
}

TEST(ToolCallingLoop, ExecCapableIsGatedByCallback) {
    auto reg = build_registry();
    auto h = make_harness();
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(
            R"({"tool_calls":[{"name":"write_file","args":{"path":"x"}}]})"),
        reg, h,
        [](ApprovalClass k, std::string_view) { return k == ApprovalClass::Readonly; });
    EXPECT_EQ(out.dispatched, 0u);
    EXPECT_EQ(out.denied, 1u);
    ASSERT_EQ(h.messages().size(), 1u);
    auto m = nlohmann::json::parse(h.messages().front().content);
    EXPECT_EQ(m["status"].get<std::string>(), "denied");
}

TEST(ToolCallingLoop, ControlPlaneCallClassifiesAndDispatchesWhenApproved) {
    auto reg = build_registry();
    auto h = make_harness();
    bool saw_control_plane = false;
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(
            R"({"tool_calls":[{"name":"set_strict_mode","args":{"enabled":true}}]})"),
        reg, h,
        [&](ApprovalClass k, std::string_view) {
            // set_strict_mode is an unknown verb -> ExecCapable by name heuristic.
            // ControlPlane only fires from required_scope; this call has none.
            saw_control_plane = (k == ApprovalClass::ControlPlane);
            return true;
        });
    EXPECT_EQ(out.dispatched, 1u);
    EXPECT_FALSE(saw_control_plane);  // documents the name-only classifier
}

TEST(ToolCallingLoop, MissingToolReturnsError) {
    auto reg = build_registry();
    auto h = make_harness();
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(
            R"({"tool_calls":[{"name":"no_such_tool","args":{}}]})"),
        reg, h,
        [](ApprovalClass, std::string_view) { return true; });
    EXPECT_EQ(out.errored, 1u);
    ASSERT_EQ(h.messages().size(), 1u);
    auto m = nlohmann::json::parse(h.messages().front().content);
    EXPECT_EQ(m["status"].get<std::string>(), "error");
}

TEST(ToolCallingLoop, MultiCallTurnDispatchesAllInOrder) {
    auto reg = build_registry();
    auto h = make_harness();
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(R"({"tool_calls":[
            {"name":"list_files","args":{"path":"a"}},
            {"name":"write_file","args":{"path":"b"}},
            {"name":"list_files","args":{"path":"c"}}]})"),
        reg, h,
        [](ApprovalClass, std::string_view) { return true; });
    EXPECT_EQ(out.dispatched, 3u);
    ASSERT_EQ(h.messages().size(), 3u);
}

TEST(ToolCallingLoop, NoToolCallsKeyIsNoOp) {
    auto reg = build_registry();
    auto h = make_harness();
    const auto out = dispatch_tool_calls(
        nlohmann::json::parse(R"({"final":"done"})"),
        reg, h,
        [](ApprovalClass, std::string_view) { return true; });
    EXPECT_EQ(out.dispatched, 0u);
    EXPECT_EQ(out.denied, 0u);
    EXPECT_EQ(out.errored, 0u);
    EXPECT_TRUE(h.messages().empty());
}

// Full mini-loop driving AgentLoopHarness across a 3-turn script.
TEST(ToolCallingLoop, EndToEndThreeTurnScriptTerminatesOnFinal) {
    auto reg = build_registry();
    auto h = make_harness();

    std::vector<std::string> script{
        R"({"tool_calls":[{"name":"list_files","args":{"path":"src"}}]})",
        R"({"tool_calls":[{"name":"write_file","args":{"path":"x","content":"hi"}}]})",
        R"({"final":"done"})",
    };
    std::size_t step = 0;
    bool finalised = false;
    std::size_t turns_run = 0;

    for (std::size_t turn = 0; !finalised && turn < script.size(); ++turn) {
        const auto tr = h.run_turn([&] {
            ProviderResponse r{};
            r.success = true;
            r.output  = script[step++];
            return r;
        });
        ASSERT_TRUE(tr.budget_consumed);
        ++turns_run;
        auto parsed = nlohmann::json::parse(tr.provider_response.output);
        if (parsed.contains("final")) { finalised = true; break; }
        dispatch_tool_calls(parsed, reg, h,
            [](ApprovalClass, std::string_view) { return true; });
    }

    EXPECT_TRUE(finalised);
    EXPECT_EQ(turns_run, 3u);
    // 2 tool-result messages appended from the two dispatched turns
    // (encoded as Assistant-role messages with JSON {tool,status,result}).
    std::size_t tool_msgs = 0;
    for (const auto& m : h.messages()) {
        if (m.role != Role::Assistant) continue;
        if (auto p = nlohmann::json::parse(m.content, nullptr, false);
            !p.is_discarded() && p.contains("tool") && p.contains("status")) {
            ++tool_msgs;
        }
    }
    EXPECT_EQ(tool_msgs, 2u);
    // Budget burned: 3 turns out of 10.
    EXPECT_EQ(h.budget().capacity() - h.budget().remaining(), 3u);
}

} // namespace
} // namespace euxis::runtime
