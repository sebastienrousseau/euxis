/// @file
/// @brief End-to-end SDK example: closed tool-calling loop driving
///        AgentLoopHarness with classify_approval gating and a
///        local IToolRegistry implementation.
///
/// The Euxis runtime ships an AgentLoopHarness that bounds iterations,
/// recommends compaction, and routes a provider call — but it deliberately
/// stops at the provider response. Closing the agentic loop (parse tool
/// calls, classify, gate, dispatch, re-feed) is the consumer's job. This
/// example is the canonical pattern: copy `dispatch_tool_calls` into your
/// own application and swap in your real provider.
///
/// What this example shows.
/// The example registers four tools that span every ApprovalClass band
/// (readonly, exec_capable, control_plane), runs them through a mock
/// provider that emits a four-turn script, and prints the resulting
/// per-turn decision trace. Every tool call passes through
/// `classify_approval`, then an ApprovalCallback the caller installs.
///
/// Build:  configure with -DEUXIS_BUILD_EXAMPLES=ON
/// Run:    ./cmake-build/docs/examples/cpp/tool_calling_loop/euxis_example_tool_calling_loop

#include <cstddef>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/agent_session.hpp"   // IToolRegistry, ToolDeclaration, ToolHandler
#include "euxis/runtime/tool_manifest.hpp"   // ApprovalClass, classify_approval

namespace {

using euxis::runtime::ApprovalClass;
using euxis::runtime::approval_class_name;
using euxis::runtime::AgentLoopHarness;
using euxis::runtime::ConversationMessage;
using euxis::runtime::IToolRegistry;
using euxis::runtime::ProviderResponse;
using euxis::runtime::Role;
using euxis::runtime::SessionMessage;
using euxis::runtime::ToolDeclaration;
using euxis::runtime::ToolDeclaration_v2;
using euxis::runtime::ToolHandler;
using euxis::runtime::TurnResult;

// ---------------------------------------------------------------------------
// LocalToolRegistry — minimal IToolRegistry impl for SDK consumers.
//
// The runtime library ships an InMemoryToolRegistry, but the factory is
// not exported. Copying this 30-line implementation is the recommended
// SDK pattern: you keep ownership of the registry, you control the
// dispatch policy, and you can swap in your own observability hooks.
// ---------------------------------------------------------------------------
class LocalToolRegistry final : public IToolRegistry {
public:
    void register_tool(ToolDeclaration decl, ToolHandler handler) override {
        std::string name = decl.name;
        tools_[std::move(name)] = {std::move(decl), std::move(handler)};
    }

    auto invoke(const std::string& name, const nlohmann::json& input)
        -> std::expected<nlohmann::json, std::string> override {
        if (auto it = tools_.find(name); it != tools_.end()) {
            return it->second.handler(input);
        }
        return std::unexpected("Tool not found: " + name);
    }

    auto list_tools() const -> std::vector<ToolDeclaration> override {
        std::vector<ToolDeclaration> out;
        out.reserve(tools_.size());
        for (const auto& [_, e] : tools_) out.push_back(e.decl);
        return out;
    }

    void remove_tool(const std::string& name) override {
        tools_.erase(name);
    }

private:
    struct Entry { ToolDeclaration decl; ToolHandler handler; };
    std::unordered_map<std::string, Entry> tools_;
};

// ---------------------------------------------------------------------------
// dispatch_tool_calls — the heart of the closed loop.
//
// Walks a JSON `tool_calls` array, classifies each one with
// classify_approval (libs/runtime/tool_manifest.hpp), asks the supplied
// ApprovalCallback for permission, invokes the tool via the registry,
// and appends a synthetic Tool-role message to the harness so the next
// turn can see the result. Returns the count of successfully dispatched
// calls — zero means "no work happened, terminate the loop".
//
// This function is intentionally small and free-standing so SDK
// consumers can copy it verbatim into their own runtime.
// ---------------------------------------------------------------------------
using ApprovalCallback = std::function<bool(ApprovalClass, std::string_view)>;

struct DispatchOutcome {
    std::size_t dispatched{0};
    std::size_t denied{0};
    std::size_t errored{0};
};

[[nodiscard]] auto dispatch_tool_calls(const nlohmann::json& provider_output,
                                      IToolRegistry& registry,
                                      AgentLoopHarness& harness,
                                      const ApprovalCallback& approve)
    -> DispatchOutcome {
    DispatchOutcome out;
    if (!provider_output.contains("tool_calls")) return out;

    for (const auto& call : provider_output.at("tool_calls")) {
        const auto name = call.value("name", std::string{});
        const auto args = call.value("args", nlohmann::json::object());

        ToolDeclaration_v2 decl_v2;
        decl_v2.name = name;
        const auto klass = euxis::runtime::classify_approval(decl_v2);

        if (!approve(klass, name)) {
            std::cout << "  - " << name
                      << " [" << approval_class_name(klass) << "] DENIED\n";
            ++out.denied;
            SessionMessage tool_msg{};
            tool_msg.role     = Role::Assistant;
            tool_msg.agent_id = harness.agent_id();
            tool_msg.content  = nlohmann::json{
                {"tool", name}, {"status", "denied"}}.dump();
            harness.add_message(ConversationMessage{tool_msg});
            continue;
        }

        auto result = registry.invoke(name, args);
        SessionMessage tool_msg{};
        tool_msg.role     = Role::Assistant;
        tool_msg.agent_id = harness.agent_id();

        if (result.has_value()) {
            std::cout << "  - " << name
                      << " [" << approval_class_name(klass) << "] OK\n";
            tool_msg.content = nlohmann::json{
                {"tool", name}, {"status", "ok"},
                {"result", *result}}.dump();
            ++out.dispatched;
        } else {
            std::cout << "  - " << name
                      << " [" << approval_class_name(klass)
                      << "] ERROR: " << result.error() << '\n';
            tool_msg.content = nlohmann::json{
                {"tool", name}, {"status", "error"},
                {"error", result.error()}}.dump();
            ++out.errored;
        }
        harness.add_message(ConversationMessage{tool_msg});
    }
    return out;
}

// ---------------------------------------------------------------------------
// MockProvider — a four-turn deterministic script.
//
// Real consumers hand AgentLoopHarness::run_turn a closure that calls
// Anthropic, OpenAI, or Ollama. The example replaces that closure with
// a deterministic script so the demo runs without network or API keys.
// ---------------------------------------------------------------------------
class MockProvider {
public:
    [[nodiscard]] auto next() -> ProviderResponse {
        ProviderResponse r{};
        r.success = true;
        r.input_tokens  = 180;
        r.output_tokens = 90;
        r.duration_ms   = 12.0;
        r.output        = scripts_[step_ < scripts_.size()
                                       ? step_++
                                       : scripts_.size() - 1];
        return r;
    }

private:
    std::size_t step_{0};
    std::vector<std::string> scripts_{
        // Turn 1: a readonly recon call (auto-approved).
        R"({"tool_calls":[{"name":"list_files","args":{"path":"src/"}}]})",
        // Turn 2: a readonly scan call (auto-approved).
        R"({"tool_calls":[{"name":"scan","args":{"path":"src/","depth":2}}]})",
        // Turn 3: an exec_capable write + a control_plane policy toggle.
        R"({"tool_calls":[
              {"name":"write_file","args":{"path":"out.txt","content":"hi"}},
              {"name":"set_strict_mode","args":{"enabled":true}}
           ]})",
        // Turn 4: model emits a final summary; the loop terminates.
        R"({"final":"audit complete: 2 files scanned, 1 write, 1 policy change"})",
    };
};

void print_messages_tail(const std::vector<ConversationMessage>& msgs,
                         std::size_t tail = 3) {
    const auto start = msgs.size() > tail ? msgs.size() - tail : 0u;
    for (auto i = start; i < msgs.size(); ++i) {
        std::cout << "  [" << i << "] (role=" << static_cast<int>(msgs[i].role)
                  << ") " << msgs[i].content.substr(0, 80)
                  << (msgs[i].content.size() > 80 ? "..." : "") << '\n';
    }
}

} // namespace

auto main() -> int {
    // -----------------------------------------------------------------------
    // 1. Registry: four tools spanning every ApprovalClass band.
    // -----------------------------------------------------------------------
    LocalToolRegistry registry;
    registry.register_tool(
        {.name = "list_files", .description = "List entries in a directory",
         .input_schema = {{"type","object"},{"properties",
            {{"path",{{"type","string"}}}}}}},
        [](const nlohmann::json& args)
            -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{
                {"path",  args.value("path", "")},
                {"entries", {"main.cpp", "CMakeLists.txt", "README.md"}}};
        });
    registry.register_tool(
        {.name = "scan", .description = "Scan a path for findings",
         .input_schema = {{"type","object"},{"properties",
            {{"path",{{"type","string"}}},{"depth",{{"type","integer"}}}}}}},
        [](const nlohmann::json& args)
            -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{
                {"path",  args.value("path", "")},
                {"depth", args.value("depth", 1)},
                {"findings", 0}};
        });
    registry.register_tool(
        {.name = "write_file", .description = "Write content to a file",
         .input_schema = {{"type","object"},{"properties",
            {{"path",{{"type","string"}}},{"content",{{"type","string"}}}}}}},
        [](const nlohmann::json& args)
            -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{
                {"path",  args.value("path", "")},
                {"bytes", args.value("content", "").size()}};
        });
    registry.register_tool(
        {.name = "set_strict_mode",
         .description = "Toggle strict policy enforcement",
         .input_schema = {{"type","object"},{"properties",
            {{"enabled",{{"type","boolean"}}}}}}},
        [](const nlohmann::json& args)
            -> std::expected<nlohmann::json, std::string> {
            return nlohmann::json{{"enabled", args.value("enabled", false)}};
        });

    // -----------------------------------------------------------------------
    // 2. Approval policy: permissive auditor. Approves everything,
    //    but logs the class so the operator can audit later. Swap in
    //    your own gate to deny exec_capable or control_plane in prod.
    // -----------------------------------------------------------------------
    const ApprovalCallback permissive_auditor =
        [](ApprovalClass klass, std::string_view name) {
            std::cout << "    [approval] " << name
                      << " classified as " << approval_class_name(klass) << '\n';
            return true;
        };

    // -----------------------------------------------------------------------
    // 3. Harness: 10-turn budget, 16 KiB context window.
    // -----------------------------------------------------------------------
    AgentLoopHarness::Config cfg;
    cfg.session_id           = "tool-loop-demo-001";
    cfg.agent_id             = "tool-loop-demo";
    cfg.iteration_budget_max = 10;
    cfg.context_window       = 16'384;
    AgentLoopHarness harness{std::move(cfg)};

    SessionMessage system{};
    system.role     = Role::System;
    system.content  = "You audit repositories. Use tools, then summarise.";
    system.agent_id = "tool-loop-demo";
    harness.add_message(ConversationMessage{system});

    SessionMessage user{};
    user.role     = Role::User;
    user.content  = "Audit src/ and toggle strict mode if you find issues.";
    user.agent_id = "tool-loop-demo";
    harness.add_message(ConversationMessage{user});

    // -----------------------------------------------------------------------
    // 4. The closed loop.
    // -----------------------------------------------------------------------
    MockProvider provider;
    std::size_t total_dispatched = 0;
    bool finalised = false;

    for (std::size_t turn = 0; !finalised && turn < 10; ++turn) {
        std::cout << "\n--- Turn " << turn << " ---\n";
        const auto result = harness.run_turn(
            [&provider] { return provider.next(); });

        if (!result.budget_consumed) {
            std::cout << "Budget exhausted.\n";
            break;
        }
        if (!result.provider_response.success) {
            std::cout << "Provider failed: "
                      << result.provider_response.error << '\n';
            break;
        }

        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(result.provider_response.output);
        } catch (const nlohmann::json::parse_error& e) {
            std::cout << "Bad provider output: " << e.what() << '\n';
            break;
        }

        if (parsed.contains("final")) {
            std::cout << "Final: " << parsed["final"].get<std::string>() << '\n';
            finalised = true;
            break;
        }

        const auto outcome = dispatch_tool_calls(
            parsed, registry, harness, permissive_auditor);
        total_dispatched += outcome.dispatched;
        std::cout << "  dispatched=" << outcome.dispatched
                  << "  denied="    << outcome.denied
                  << "  errored="   << outcome.errored << '\n';

        if (outcome.dispatched == 0 && outcome.denied == 0
            && outcome.errored == 0) {
            std::cout << "  (no tool calls; assistant must finalise next turn)\n";
        }
    }

    // -----------------------------------------------------------------------
    // 5. Summary.
    // -----------------------------------------------------------------------
    std::cout << "\n=== Summary ===\n"
              << "tool calls dispatched: " << total_dispatched << '\n'
              << "iterations remaining:  " << harness.budget().remaining()
              << " / " << harness.budget().capacity() << '\n'
              << "messages in transcript: " << harness.messages().size()
              << '\n'
              << "tail of transcript:\n";
    print_messages_tail(harness.messages(), 4);

    std::cout << "\nDemo completed " << (finalised ? "with" : "without")
              << " final summary.\n";
    return finalised ? 0 : 1;
}
