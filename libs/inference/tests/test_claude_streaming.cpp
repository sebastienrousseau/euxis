/// @file
/// @brief End-to-end tests for ClaudeStreamingProvider against a
///        local mock httplib::Server that emits Anthropic-format SSE
///        events. The provider's coroutine path is gated by
///        EUXIS_HAS_STD_GENERATOR; on toolchains without `<generator>`
///        the tests fall through to a GTEST_SKIP.

#include <gtest/gtest.h>

#include "euxis/inference/claude_streaming.hpp"

#include "euxis/runtime/streaming.hpp"

#if !defined(EUXIS_HAS_STD_GENERATOR)

namespace {
TEST(ClaudeStreaming, GuardedOutOnThisToolchain) {
    GTEST_SKIP() << "<generator> not available — ClaudeStreamingProvider compiled out";
}
} // namespace

#else

#include <future>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace euxis::inference {
namespace {

using runtime::DeltaKind;
using runtime::ProviderDelta;

// ---------------------------------------------------------------------------
// MockSseServer — spins up an httplib::Server on a random port that
// streams a preset SSE body on POST /v1/messages.
// ---------------------------------------------------------------------------
struct MockSseServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    explicit MockSseServer(std::string sse_body) {
        srv.Post("/v1/messages",
                 [body = std::move(sse_body)](const httplib::Request&,
                                              httplib::Response& res) {
            res.status = 200;
            res.set_content(body, "text/event-stream");
        });
        std::promise<int> bound;
        auto fut = bound.get_future();
        th = std::thread([this, &bound]() {
            int p = srv.bind_to_any_port("127.0.0.1");
            bound.set_value(p);
            srv.listen_after_bind();
        });
        port = fut.get();
    }
    ~MockSseServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

[[nodiscard]] auto make_config(const std::string& base_url) -> ClaudeStreamingConfig {
    ClaudeStreamingConfig cfg;
    cfg.id           = "claude-test";
    cfg.model        = "claude-sonnet-4-6";
    cfg.base_url     = base_url;
    cfg.api_key      = "test-key";  // explicit so env-var lookup is skipped
    cfg.env_var_name = "ANTHROPIC_API_KEY";
    cfg.max_tokens   = 256;
    return cfg;
}

[[nodiscard]] auto drain(ClaudeStreamingProvider& p,
                        const std::string& prompt) -> std::vector<ProviderDelta> {
    std::vector<ProviderDelta> out;
    for (auto&& d : p.execute_stream("claude-sonnet-4-6", prompt)) {
        out.push_back(std::move(d));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Text-only stream: three text_delta events + a usage record.
// ---------------------------------------------------------------------------
constexpr auto kTextOnlySse = R"sse(event: message_start
data: {"type":"message_start","message":{"id":"msg_01"}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello "}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"world"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"."}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"input_tokens":42,"output_tokens":3}}

event: message_stop
data: {"type":"message_stop"}

)sse";

TEST(ClaudeStreaming, TextOnlyStreamYieldsExpectedDeltas) {
    MockSseServer mock{kTextOnlySse};
    ClaudeStreamingProvider p{make_config(mock.base_url())};

    auto deltas = drain(p, "hi");
    ASSERT_EQ(deltas.size(), 4u) << "3 text fragments + 1 usage";

    EXPECT_EQ(deltas[0].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[0].text, "Hello ");
    EXPECT_EQ(deltas[1].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[1].text, "world");
    EXPECT_EQ(deltas[2].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[2].text, ".");
    EXPECT_EQ(deltas[3].kind, DeltaKind::Usage);
    EXPECT_EQ(deltas[3].input_tokens,  42);
    EXPECT_EQ(deltas[3].output_tokens, 3);
}

// ---------------------------------------------------------------------------
// Tool-call stream: text first, then a tool_use block whose input
// JSON arrives split across two partial_json deltas.
// ---------------------------------------------------------------------------
constexpr auto kToolCallSse = R"sse(event: message_start
data: {"type":"message_start","message":{"id":"msg_02"}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"I will scan."}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: content_block_start
data: {"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"toolu_42","name":"scan","input":{}}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"{\"path\":\"src"}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"/\",\"depth\":2}"}}

event: content_block_stop
data: {"type":"content_block_stop","index":1}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"tool_use"},"usage":{"input_tokens":210,"output_tokens":18}}

event: message_stop
data: {"type":"message_stop"}

)sse";

TEST(ClaudeStreaming, ToolCallPartialsCarryIdAndNameAndReassemble) {
    MockSseServer mock{kToolCallSse};
    ClaudeStreamingProvider p{make_config(mock.base_url())};

    auto deltas = drain(p, "scan src");
    ASSERT_EQ(deltas.size(), 4u) << "1 text + 2 tool partials + 1 usage";

    EXPECT_EQ(deltas[0].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[0].text, "I will scan.");

    EXPECT_EQ(deltas[1].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[1].tool_call_id,   "toolu_42");
    EXPECT_EQ(deltas[1].tool_call_name, "scan");
    EXPECT_EQ(deltas[2].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[2].tool_call_id,   "toolu_42");
    EXPECT_EQ(deltas[2].tool_call_name, "scan");
    const auto reassembled = deltas[1].text + deltas[2].text;
    EXPECT_EQ(reassembled, R"({"path":"src/","depth":2})");

    EXPECT_EQ(deltas[3].kind, DeltaKind::Usage);
    EXPECT_EQ(deltas[3].input_tokens,  210);
    EXPECT_EQ(deltas[3].output_tokens, 18);
}

// ---------------------------------------------------------------------------
// Empty API key — provider emits no deltas and the generator ends
// cleanly (matches the IStreamingProvider contract: no error channel,
// caller checks the config before iterating if it cares).
// ---------------------------------------------------------------------------
TEST(ClaudeStreaming, EmptyApiKeyYieldsEmptyStream) {
    ClaudeStreamingConfig cfg;
    cfg.base_url     = "http://127.0.0.1:1";  // never connected to
    cfg.api_key      = "";
    cfg.env_var_name = "DEFINITELY_NOT_SET_FOR_THIS_TEST_2026";
    // Defensive: scrub the env if a developer happened to export this.
    unsetenv(cfg.env_var_name.c_str());

    ClaudeStreamingProvider p{cfg};
    auto deltas = drain(p, "hi");
    EXPECT_TRUE(deltas.empty());
}

// ---------------------------------------------------------------------------
// Non-2xx response — generator ends after whatever buffered records
// existed (none in this case since the body is non-SSE).
// ---------------------------------------------------------------------------
struct MockErrorServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    MockErrorServer() {
        srv.Post("/v1/messages",
                 [](const httplib::Request&, httplib::Response& res) {
            res.status = 500;
            res.set_content("{\"error\":\"oops\"}", "application/json");
        });
        std::promise<int> bound;
        auto fut = bound.get_future();
        th = std::thread([this, &bound]() {
            int p = srv.bind_to_any_port("127.0.0.1");
            bound.set_value(p);
            srv.listen_after_bind();
        });
        port = fut.get();
    }
    ~MockErrorServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

TEST(ClaudeStreaming, Non2xxResponseYieldsEmptyStream) {
    MockErrorServer mock;
    ClaudeStreamingProvider p{make_config(mock.base_url())};

    auto deltas = drain(p, "hi");
    EXPECT_TRUE(deltas.empty());
}

} // namespace
} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
