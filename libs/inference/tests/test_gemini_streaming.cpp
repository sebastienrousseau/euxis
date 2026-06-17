/// @file
/// @brief Tests for GeminiStreamingProvider against a local mock
///        httplib::Server emitting Gemini-format SSE events.

#include <gtest/gtest.h>

#include "euxis/inference/gemini_streaming.hpp"
#include "euxis/runtime/streaming.hpp"

#if !defined(EUXIS_HAS_STD_GENERATOR)

namespace {
TEST(GeminiStreaming, GuardedOutOnThisToolchain) {
    GTEST_SKIP() << "<generator> not available — GeminiStreamingProvider compiled out";
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

struct MockGeminiServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    explicit MockGeminiServer(std::string sse_body) {
        // Gemini's path carries the model name + ":streamGenerateContent",
        // which the mock matches via a wildcard handler.
        srv.Post(R"(/v1beta/models/[^/]+:streamGenerateContent)",
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
    ~MockGeminiServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }
    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

[[nodiscard]] auto make_config(const std::string& base_url) -> GeminiStreamingConfig {
    GeminiStreamingConfig cfg;
    cfg.id           = "gemini-test";
    cfg.model        = "gemini-2.5-pro";
    cfg.base_url     = base_url;
    cfg.api_key      = "test-key";
    cfg.env_var_name = "GEMINI_API_KEY";
    return cfg;
}

[[nodiscard]] auto drain(GeminiStreamingProvider& p,
                        const std::string& prompt) -> std::vector<ProviderDelta> {
    std::vector<ProviderDelta> out;
    for (auto&& d : p.execute_stream("gemini-2.5-pro", prompt)) {
        out.push_back(std::move(d));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Text-only stream: three text parts spread across three SSE frames,
// then a final usageMetadata-carrying frame.
// ---------------------------------------------------------------------------
constexpr auto kTextOnlySse = R"sse(data: {"candidates":[{"content":{"parts":[{"text":"Hello "}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"text":"world"}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"text":"."}],"role":"model"},"index":0,"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":42,"candidatesTokenCount":3,"totalTokenCount":45}}

)sse";

TEST(GeminiStreaming, TextOnlyStreamYieldsExpectedDeltas) {
    MockGeminiServer mock{kTextOnlySse};
    GeminiStreamingProvider p{make_config(mock.base_url())};

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
// Tool-call stream: Gemini emits the functionCall object fully-formed
// in a single frame — there's no partial-args streaming. The provider
// emits one ToolCallPartial whose text carries the complete args JSON.
// ---------------------------------------------------------------------------
constexpr auto kToolCallSse = R"sse(data: {"candidates":[{"content":{"parts":[{"text":"I will scan."}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"functionCall":{"name":"scan","args":{"path":"src/","depth":2}}}],"role":"model"},"index":0,"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":210,"candidatesTokenCount":18,"totalTokenCount":228}}

)sse";

TEST(GeminiStreaming, FunctionCallEmitsSingleFullyFormedPartial) {
    MockGeminiServer mock{kToolCallSse};
    GeminiStreamingProvider p{make_config(mock.base_url())};

    auto deltas = drain(p, "scan src");
    // 1 text + 1 fully-formed tool partial + 1 usage = 3
    ASSERT_EQ(deltas.size(), 3u);

    EXPECT_EQ(deltas[0].kind, DeltaKind::TextFragment);
    EXPECT_EQ(deltas[0].text, "I will scan.");

    EXPECT_EQ(deltas[1].kind, DeltaKind::ToolCallPartial);
    EXPECT_EQ(deltas[1].tool_call_name, "scan");
    EXPECT_EQ(deltas[1].tool_call_id,   "gemini-fc-scan");  // synthetic
    EXPECT_EQ(deltas[1].text, R"({"depth":2,"path":"src/"})")
        << "nlohmann::json serialises object members alphabetically";

    EXPECT_EQ(deltas[2].kind, DeltaKind::Usage);
    EXPECT_EQ(deltas[2].input_tokens,  210);
    EXPECT_EQ(deltas[2].output_tokens, 18);
}

TEST(GeminiStreaming, EmptyApiKeyYieldsEmptyStream) {
    GeminiStreamingConfig cfg;
    cfg.base_url     = "http://127.0.0.1:1";
    cfg.api_key      = "";
    cfg.env_var_name = "DEFINITELY_NOT_SET_FOR_THIS_TEST_2026";
    unsetenv(cfg.env_var_name.c_str());

    GeminiStreamingProvider p{cfg};
    auto deltas = drain(p, "hi");
    EXPECT_TRUE(deltas.empty());
}

} // namespace
} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
