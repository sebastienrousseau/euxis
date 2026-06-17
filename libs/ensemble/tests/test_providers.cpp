#include <gtest/gtest.h>

#include "euxis/ensemble/providers/claude.hpp"
#include "euxis/ensemble/providers/gemini.hpp"
#include "euxis/ensemble/providers/openai.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>

namespace euxis::ensemble::providers {
namespace {

using namespace std::chrono_literals;

/// Spins up an httplib::Server on a random localhost port that
/// echoes back a configured response body for `POST <path>`.
/// Used so the provider HTTP path is exercised end-to-end without
/// hitting the real public APIs.
struct MockServer {
    httplib::Server srv;
    std::thread     th;
    int             port{0};

    MockServer(const std::string& path,
               std::function<httplib::Response(const httplib::Request&)> handler) {
        srv.Post(path, [handler](const httplib::Request& req,
                                  httplib::Response& res) {
            auto out = handler(req);
            res.status = out.status;
            res.body   = out.body;
            for (const auto& [k, v] : out.headers) {
                res.set_header(k, v);
            }
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

    ~MockServer() {
        srv.stop();
        if (th.joinable()) th.join();
    }

    [[nodiscard]] auto base_url() const -> std::string {
        return "http://127.0.0.1:" + std::to_string(port);
    }
};

auto valid_assistant_reply() -> std::string {
    // The text returned by extract_text(); shaped to satisfy
    // libs/ensemble's parse_response() contract.
    return R"({"true_positive": true, "confidence": 0.85,
               "rationale": "verified"})";
}

auto make_vote_request() -> VoteRequest {
    VoteRequest r;
    r.rule_id     = "rule.x";
    r.message     = "look at me";
    r.snippet     = "eval(input)";
    r.source_path = "a/b.py";
    r.start_row   = 3;
    r.start_column = 5;
    r.severity   = euxis::security::Severity::High;
    return r;
}

// =====================================================================
// Generic HttpVerifier behaviour — auth missing, transport errors.
// =====================================================================

TEST(HttpVerifier, EmptyKeyShortCircuits) {
    auto cfg     = default_claude_config();
    cfg.api_key  = "";
    cfg.env_var_name = "EUXIS_TEST_NO_SUCH_KEY_PLEASE";
    // Make sure the env var is truly absent for the test.
    ::unsetenv("EUXIS_TEST_NO_SUCH_KEY_PLEASE");
    ClaudeVerifier v{cfg};
    auto out = v.vote(make_vote_request());
    EXPECT_FALSE(out.vote.true_positive);
    EXPECT_NE(out.rationale.find("auth missing"), std::string::npos);
}

// =====================================================================
// Claude — mock server roundtrip.
// =====================================================================

TEST(ClaudeVerifier, RoundtripReturnsConfirmed) {
    nlohmann::json claude_resp;
    claude_resp["content"] = nlohmann::json::array({
        {{"type", "text"}, {"text", valid_assistant_reply()}},
    });
    MockServer mock{"/v1/messages",
        [resp = claude_resp.dump()](const httplib::Request& req) {
            httplib::Response r;
            r.status = 200;
            r.body   = resp;
            EXPECT_NE(req.get_header_value("x-api-key").size(), 0U);
            EXPECT_EQ(req.get_header_value("anthropic-version"),
                      "2023-06-01");
            return r;
        }};

    auto cfg     = default_claude_config();
    cfg.base_url = mock.base_url();
    cfg.api_key  = "test-key";
    ClaudeVerifier v{cfg};

    auto out = v.vote(make_vote_request());
    EXPECT_EQ(out.vote.provider, "claude");
    EXPECT_TRUE(out.vote.true_positive);
    EXPECT_GT(out.vote.confidence, 0.5F);
}

TEST(ClaudeVerifier, ServerErrorTriggersBoundedRetry) {
    std::atomic<int> hits{0};
    MockServer mock{"/v1/messages",
        [&hits](const httplib::Request&) {
            ++hits;
            httplib::Response r;
            r.status = 500;
            r.body   = "{\"error\": \"injected\"}";
            return r;
        }};

    auto cfg          = default_claude_config();
    cfg.base_url      = mock.base_url();
    cfg.api_key       = "test-key";
    cfg.max_retries   = 2;
    cfg.timeout       = 1s;
    ClaudeVerifier v{cfg};
    auto out = v.vote(make_vote_request());
    EXPECT_FALSE(out.vote.true_positive);
    EXPECT_EQ(hits.load(), 3) << "max_retries=2 means three total attempts";
}

// =====================================================================
// OpenAI — mock server roundtrip.
// =====================================================================

TEST(OpenAIVerifier, RoundtripReturnsConfirmed) {
    nlohmann::json openai_resp;
    openai_resp["choices"] = nlohmann::json::array({
        {{"message", {{"role", "assistant"},
                      {"content", valid_assistant_reply()}}}},
    });
    MockServer mock{"/v1/chat/completions",
        [resp = openai_resp.dump()](const httplib::Request& req) {
            httplib::Response r;
            r.status = 200;
            r.body   = resp;
            EXPECT_NE(req.get_header_value("Authorization").find("Bearer"),
                      std::string::npos);
            return r;
        }};

    auto cfg     = default_openai_config();
    cfg.base_url = mock.base_url();
    cfg.api_key  = "test-key";
    OpenAIVerifier v{cfg};

    auto out = v.vote(make_vote_request());
    EXPECT_EQ(out.vote.provider, "openai");
    EXPECT_TRUE(out.vote.true_positive);
}

// =====================================================================
// Gemini — mock server roundtrip (auth via query string).
// =====================================================================

TEST(GeminiVerifier, RoundtripReturnsConfirmed) {
    nlohmann::json gemini_resp;
    gemini_resp["candidates"] = nlohmann::json::array({
        {{"content",
            {{"parts", nlohmann::json::array({
                {{"text", valid_assistant_reply()}}})}}}}});
    MockServer mock{"/v1beta/models/gemini-test:generateContent",
        [resp = gemini_resp.dump()](const httplib::Request& req) {
            httplib::Response r;
            r.status = 200;
            r.body   = resp;
            // Gemini auth lives in the query string ?key=...
            EXPECT_NE(req.get_param_value("key").size(), 0U);
            return r;
        }};

    auto cfg     = default_gemini_config();
    cfg.base_url = mock.base_url();
    cfg.api_key  = "test-key";
    cfg.model    = "gemini-test";
    GeminiVerifier v{cfg};

    auto out = v.vote(make_vote_request());
    EXPECT_EQ(out.vote.provider, "gemini");
    EXPECT_TRUE(out.vote.true_positive);
}

} // namespace
} // namespace euxis::ensemble::providers
