/// @file
/// @brief Tests for OpenAIStreamingProvider against a local mock
///        httplib::Server emitting OpenAI-format SSE events.

#include <gtest/gtest.h>

#include "euxis/inference/openai_streaming.hpp"
#include "euxis/runtime/streaming.hpp"

#if !defined(EUXIS_HAS_STD_GENERATOR)

namespace {
TEST(OpenAIStreaming, GuardedOutOnThisToolchain) {
    GTEST_SKIP() << "<generator> not available — OpenAIStreamingProvider compiled out";
}
} // namespace

#else

#include <future>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>

namespace euxis::inference {
namespace {

using runtime::DeltaKind;
using runtime::ProviderDelta;

struct MockOpenAIServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    explicit MockOpenAIServer(std::string sse_body) {
        srv.Post("/v1/chat/completions",
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
    ~MockOpenAIServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

[[nodiscard]] auto make_config(const std::string& base_url) -> OpenAIStreamingConfig {
    OpenAIStreamingConfig cfg;
    cfg.id           = "openai-test";
    cfg.model        = "gpt-4o";
    cfg.base_url     = base_url;
    cfg.api_key      = "sk-test";
    cfg.env_var_name = "OPENAI_API_KEY";
    return cfg;
}

[[nodiscard]] auto drain(OpenAIStreamingProvider& p,
                        const std::string& prompt) -> std::vector<ProviderDelta> {
    std::vector<ProviderDelta> out;
    for (auto&& d : p.execute_stream("gpt-4o", prompt)) {
        out.push_back(std::move(d));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Text-only stream: three content deltas + final usage chunk + [DONE].
// ---------------------------------------------------------------------------
constexpr auto kTextOnlySse = R"sse(data: {"choices":[{"delta":{"role":"assistant","content":""},"index":0}]}

data: {"choices":[{"delta":{"content":"Hello "},"index":0}]}

data: {"choices":[{"delta":{"content":"world"},"index":0}]}

data: {"choices":[{"delta":{"content":"."},"index":0}]}

data: {"choices":[{"delta":{},"index":0,"finish_reason":"stop"}]}

data: {"choices":[],"usage":{"prompt_tokens":42,"completion_tokens":3,"total_tokens":45}}

data: [DONE]

)sse";

TEST(OpenAIStreaming, TextOnlyStreamYieldsExpectedDeltas) {
    MockOpenAIServer mock{kTextOnlySse};
    OpenAIStreamingProvider p{make_config(mock.base_url())};

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
// Tool-call stream: one text fragment, then a tool_call whose id+name
// arrive on the first chunk and arguments stream across three
// subsequent chunks (no id/name repeated — provider must carry them
// forward by index).
// ---------------------------------------------------------------------------
constexpr auto kToolCallSse = R"sse(data: {"choices":[{"delta":{"role":"assistant","content":""},"index":0}]}

data: {"choices":[{"delta":{"content":"I will scan."},"index":0}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call_99","type":"function","function":{"name":"scan","arguments":""}}]},"index":0}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"path\":\"src"}}]},"index":0}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"/\",\"depth\":2}"}}]},"index":0}]}

data: {"choices":[{"delta":{},"index":0,"finish_reason":"tool_calls"}]}

data: {"choices":[],"usage":{"prompt_tokens":210,"completion_tokens":18,"total_tokens":228}}

data: [DONE]

)sse";

TEST(OpenAIStreaming, ToolCallPartialsCarryIdAndNameAcrossChunks) {
    MockOpenAIServer mock{kToolCallSse};
    OpenAIStreamingProvider p{make_config(mock.base_url())};

    auto deltas = drain(p, "scan src");
    // 1 text + 1 empty-arg tool partial (from id/name chunk) + 2 arg partials + 1 usage = 5
    ASSERT_EQ(deltas.size(), 5u);

    EXPECT_EQ(deltas[0].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[0].text, "I will scan.");

    EXPECT_EQ(deltas[1].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[1].text, "");
    EXPECT_EQ(deltas[1].tool_call_id,   "call_99");
    EXPECT_EQ(deltas[1].tool_call_name, "scan");

    EXPECT_EQ(deltas[2].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[2].tool_call_id,   "call_99");
    EXPECT_EQ(deltas[2].tool_call_name, "scan");

    EXPECT_EQ(deltas[3].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[3].tool_call_id,   "call_99");
    EXPECT_EQ(deltas[3].tool_call_name, "scan");

    const auto reassembled = deltas[1].text + deltas[2].text + deltas[3].text;
    EXPECT_EQ(reassembled, R"({"path":"src/","depth":2})");

    EXPECT_EQ(deltas[4].kind, DeltaKind::Usage);
    EXPECT_EQ(deltas[4].input_tokens,  210);
    EXPECT_EQ(deltas[4].output_tokens, 18);
}

TEST(OpenAIStreaming, EmptyApiKeyYieldsEmptyStream) {
    OpenAIStreamingConfig cfg;
    cfg.base_url     = "http://127.0.0.1:1";
    cfg.api_key      = "";
    cfg.env_var_name = "DEFINITELY_NOT_SET_FOR_THIS_TEST_2026";
    unsetenv(cfg.env_var_name.c_str());

    OpenAIStreamingProvider p{cfg};
    auto deltas = drain(p, "hi");
    EXPECT_TRUE(deltas.empty());
}

} // namespace
} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
