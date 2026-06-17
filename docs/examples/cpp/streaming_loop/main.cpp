/// @file
/// @brief End-to-end SDK example: coroutine-based streaming provider
///        feeding an AgentLoopHarness, with assistant-text accumulation
///        and tool-call partial reassembly.
///
/// Real provider implementations (Anthropic, OpenAI) emit incremental
/// text and partial tool-call argument JSON across many small SSE
/// chunks. This example shows the consumer-side pattern: walk the
/// `std::generator<ProviderDelta>` returned by an IStreamingProvider,
/// accumulate text fragments into a single assistant message, group
/// tool-call partials by id, and finalise once the Usage delta arrives.
///
/// Toolchain note
/// --------------
/// `IStreamingProvider` requires `<generator>` (C++23). On toolchains
/// that don't ship it (AppleClang 21 / Homebrew LLVM 22 libc++ as of
/// 2026-06), this binary compiles to a one-line "build skipped" message
/// so the example tree builds cleanly everywhere. GCC 14 on Ubuntu has
/// `<generator>` and runs the demo end-to-end.
///
/// Build:  configure with -DEUXIS_BUILD_EXAMPLES=ON
/// Run:    ./cmake-build/docs/examples/cpp/streaming_loop/euxis_example_streaming_loop

#include <iostream>

#include "euxis/runtime/streaming.hpp"

#if !defined(EUXIS_HAS_STD_GENERATOR)

auto main() -> int {
    std::cout
        << "euxis_example_streaming_loop: skipped — this toolchain does not\n"
        << "ship <generator>. Build with GCC 14 / libstdc++ 14 to run the\n"
        << "full streaming demo.\n";
    return 0;
}

#else

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/agent_session.hpp"

namespace {

using namespace euxis::runtime;

// ---------------------------------------------------------------------------
// MockStreamingProvider — a deterministic 6-delta script that mimics
// what a real LLM SSE stream looks like: text fragments interleaved
// with tool-call argument partials, then a Usage record.
// ---------------------------------------------------------------------------
class MockStreamingProvider final : public IStreamingProvider {
public:
    auto execute_stream(const std::string& /*model*/,
                        const std::string& /*prompt*/,
                        int /*timeout_ms*/ = 30'000)
        -> std::generator<ProviderDelta> override {
        ProviderDelta d;

        // Three text fragments — assembling "I will scan src/."
        d.kind = DeltaKind::TextFragment;
        d.text = "I will ";   co_yield d;
        d.text = "scan ";     co_yield d;
        d.text = "src/.";     co_yield d;

        // Two tool-call partials for one call (the assistant emits the
        // arg JSON incrementally; the consumer reassembles by id).
        d.kind = DeltaKind::ToolCallPartial;
        d.tool_call_name = "scan";
        d.tool_call_id   = "call_42";
        d.text = R"({"path":"sr)";  co_yield d;
        d.text = R"(c/","depth":2})"; co_yield d;

        // Final usage record. Stream ends naturally on co_return.
        d.kind            = DeltaKind::Usage;
        d.text.clear();
        d.tool_call_name.clear();
        d.tool_call_id.clear();
        d.input_tokens    = 312;
        d.output_tokens   = 28;
        co_yield d;
    }
};

// ---------------------------------------------------------------------------
// drain_stream — the consumer-side pattern.
//
// Walks the generator, accumulates text into one string per assistant
// turn, reassembles tool-call argument JSON keyed by tool_call_id, and
// records final usage. Returns a struct the caller can feed into the
// agent loop.
// ---------------------------------------------------------------------------
struct DrainedTurn {
    std::string assistant_text;
    std::unordered_map<std::string, std::string> tool_args_by_id;
    std::unordered_map<std::string, std::string> tool_name_by_id;
    std::int32_t input_tokens{0};
    std::int32_t output_tokens{0};
};

auto drain_stream(IStreamingProvider& provider,
                  const std::string& model,
                  const std::string& prompt) -> DrainedTurn {
    DrainedTurn turn;
    for (auto&& d : provider.execute_stream(model, prompt)) {
        switch (d.kind) {
            case DeltaKind::TextFragment:
                turn.assistant_text += d.text;
                std::cout << d.text << std::flush;  // live print
                break;
            case DeltaKind::ToolCallPartial:
                turn.tool_args_by_id[d.tool_call_id] += d.text;
                turn.tool_name_by_id[d.tool_call_id]  = d.tool_call_name;
                break;
            case DeltaKind::Usage:
                turn.input_tokens  = d.input_tokens;
                turn.output_tokens = d.output_tokens;
                break;
            case DeltaKind::Done:
                break;
        }
    }
    std::cout << '\n';
    return turn;
}

} // namespace

auto main() -> int {
    MockStreamingProvider provider;

    // Harness for context. The streaming generator doesn't talk to it
    // directly — the consumer pattern is "drain into a turn, then
    // commit one assembled assistant message."
    AgentLoopHarness::Config cfg;
    cfg.session_id           = "stream-demo-001";
    cfg.agent_id             = "stream-demo";
    cfg.iteration_budget_max = 5;
    cfg.context_window       = 16'384;
    AgentLoopHarness harness{std::move(cfg)};

    SessionMessage user{};
    user.role     = Role::User;
    user.content  = "Scan src/ for findings.";
    user.agent_id = "stream-demo";
    harness.add_message(ConversationMessage{user});

    std::cout << "Streaming assistant turn:\n  ";
    const auto turn = drain_stream(provider, "mock-model", user.content);

    // Commit the assembled assistant text as one ConversationMessage.
    SessionMessage assistant{};
    assistant.role     = Role::Assistant;
    assistant.content  = turn.assistant_text;
    assistant.agent_id = "stream-demo";
    harness.add_message(ConversationMessage{assistant});

    std::cout << "\n=== Drain summary ===\n"
              << "assistant text:   \"" << turn.assistant_text << "\"\n"
              << "tool calls:       " << turn.tool_args_by_id.size() << '\n';
    for (const auto& [id, args] : turn.tool_args_by_id) {
        std::cout << "  - " << turn.tool_name_by_id.at(id)
                  << " (id=" << id << ") args=" << args << '\n';
    }
    std::cout << "tokens in/out:    "
              << turn.input_tokens << " / " << turn.output_tokens << '\n'
              << "transcript size:  " << harness.messages().size() << '\n';
    return 0;
}

#endif // EUXIS_HAS_STD_GENERATOR
