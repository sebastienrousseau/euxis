/// @file
/// @brief Tests for the coroutine-based streaming provider API.
///
/// The IStreamingProvider interface is only declared when the toolchain
/// ships <generator>. Tests that exercise the coroutine path are
/// compiled out on toolchains lacking it (Apple Clang 21 today) and
/// the test binary still links — matching the EUXIS_HAS_STD_GENERATOR
/// guard precedent set by libs/runtime/session_store.

#include <gtest/gtest.h>

#include <string>

#include "euxis/runtime/streaming.hpp"

namespace euxis::runtime {
namespace {

// ---------------------------------------------------------------------------
// Always-on smoke: the wire-format struct is usable regardless of
// whether the coroutine API is compiled in.
// ---------------------------------------------------------------------------
TEST(StreamingWireFormat, ProviderDeltaDefaultsToDone) {
    ProviderDelta d;
    EXPECT_EQ(d.kind, DeltaKind::Done);
    EXPECT_TRUE(d.text.empty());
    EXPECT_TRUE(d.tool_call_name.empty());
    EXPECT_TRUE(d.tool_call_id.empty());
    EXPECT_EQ(d.input_tokens, 0);
    EXPECT_EQ(d.output_tokens, 0);
}

TEST(StreamingWireFormat, AllDeltaKindsAreDistinct) {
    EXPECT_NE(DeltaKind::TextFragment,    DeltaKind::ToolCallPartial);
    EXPECT_NE(DeltaKind::TextFragment,    DeltaKind::Usage);
    EXPECT_NE(DeltaKind::TextFragment,    DeltaKind::Done);
    EXPECT_NE(DeltaKind::ToolCallPartial, DeltaKind::Usage);
    EXPECT_NE(DeltaKind::ToolCallPartial, DeltaKind::Done);
    EXPECT_NE(DeltaKind::Usage,           DeltaKind::Done);
}

#if defined(EUXIS_HAS_STD_GENERATOR)

// ---------------------------------------------------------------------------
// Mock streaming provider that emits a fixed 4-delta script. Exercises
// the IStreamingProvider contract end-to-end without network or LLM.
// ---------------------------------------------------------------------------
class MockStreamingProvider final : public IStreamingProvider {
public:
    auto execute_stream(std::string /*model*/,
                        std::string /*prompt*/,
                        int /*timeout_ms*/ = 30'000)
        -> std::generator<ProviderDelta> override {
        ProviderDelta d;
        d.kind = DeltaKind::TextFragment;
        d.text = "Hello ";
        co_yield d;

        d.text = "world.";
        co_yield d;

        d.kind = DeltaKind::ToolCallPartial;
        d.text = R"({"path":"src/"})";
        d.tool_call_name = "list_files";
        d.tool_call_id   = "call_1";
        co_yield d;

        d.kind = DeltaKind::Usage;
        d.text.clear();
        d.tool_call_name.clear();
        d.tool_call_id.clear();
        d.input_tokens  = 42;
        d.output_tokens = 7;
        co_yield d;
    }
};

TEST(StreamingProvider, MockEmitsFourDeltasInOrder) {
    MockStreamingProvider p;
    std::vector<ProviderDelta> collected;
    for (auto&& d : p.execute_stream("mock", "hi")) {
        collected.push_back(d);
    }
    ASSERT_EQ(collected.size(), 4u);
    EXPECT_EQ(collected[0].kind, DeltaKind::TextFragment);
    EXPECT_EQ(collected[0].text, "Hello ");
    EXPECT_EQ(collected[1].kind, DeltaKind::TextFragment);
    EXPECT_EQ(collected[1].text, "world.");
    EXPECT_EQ(collected[2].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(collected[2].tool_call_name, "list_files");
    EXPECT_EQ(collected[2].tool_call_id,   "call_1");
    EXPECT_EQ(collected[3].kind, DeltaKind::Usage);
    EXPECT_EQ(collected[3].input_tokens,  42);
    EXPECT_EQ(collected[3].output_tokens, 7);
}

// Consumer-side pattern: accumulate text fragments into a single
// assistant message and parse tool-call partials by id. This is the
// shape SDK consumers write when adapting the streaming API to the
// existing AgentLoopHarness::add_message convention.
TEST(StreamingProvider, AccumulateTextAndToolCallPartials) {
    MockStreamingProvider p;
    std::string assistant_text;
    std::unordered_map<std::string, std::string> tool_args_by_id;
    std::string tool_name_for_id;
    int input = 0;
    int output = 0;

    for (auto&& d : p.execute_stream("mock", "hi")) {
        switch (d.kind) {
            case DeltaKind::TextFragment:
                assistant_text += d.text;
                break;
            case DeltaKind::ToolCallPartial:
                tool_args_by_id[d.tool_call_id] += d.text;
                tool_name_for_id = d.tool_call_name;
                break;
            case DeltaKind::Usage:
                input = d.input_tokens;
                output = d.output_tokens;
                break;
            case DeltaKind::Done:
                break;
        }
    }
    EXPECT_EQ(assistant_text, "Hello world.");
    EXPECT_EQ(tool_args_by_id["call_1"], R"({"path":"src/"})");
    EXPECT_EQ(tool_name_for_id, "list_files");
    EXPECT_EQ(input, 42);
    EXPECT_EQ(output, 7);
}

#else // !EUXIS_HAS_STD_GENERATOR

TEST(StreamingProvider, GuardedOutOnThisToolchain) {
    GTEST_SKIP() << "<generator> not available — IStreamingProvider compiled out";
}

#endif

} // namespace
} // namespace euxis::runtime
