/// @file
/// @brief OpenAI Chat Completions streaming provider implementation.

#include "euxis/inference/openai_streaming.hpp"

#if defined(EUXIS_HAS_STD_GENERATOR)

#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace euxis::inference {

namespace {

using runtime::DeltaKind;
using runtime::ProviderDelta;

[[nodiscard]] auto resolve_api_key(const OpenAIStreamingConfig& cfg) -> std::string {
    if (!cfg.api_key.empty()) return cfg.api_key;
    if (!cfg.env_var_name.empty()) {
        if (const char* env = std::getenv(cfg.env_var_name.c_str())) {
            return std::string{env};
        }
    }
    return {};
}

[[nodiscard]] auto build_request_body(const OpenAIStreamingConfig& cfg,
                                     const std::string& model,
                                     const std::string& prompt) -> std::string {
    nlohmann::json body;
    body["model"]   = model.empty() ? cfg.model : model;
    body["stream"]  = true;
    // Force OpenAI to include the final usage record on the stream tail
    // (default is to omit it when streaming).
    body["stream_options"] = nlohmann::json::object({{"include_usage", true}});
    body["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}},
    });
    return body.dump();
}

// ---------------------------------------------------------------------------
// SSE parser — same shape as the Claude one; OpenAI also emits `data:`
// records separated by `\n\n`. Extra requirement: a final literal
// `data: [DONE]` marker terminates the stream.
// ---------------------------------------------------------------------------
class SseParser {
public:
    template <typename Handler>
    void feed(std::string_view bytes, const Handler& on_event) {
        buf_.append(bytes);
        std::size_t pos = 0;
        while (true) {
            const auto end = buf_.find("\n\n", pos);
            if (end == std::string::npos) {
                buf_.erase(0, pos);
                return;
            }
            std::string_view record{buf_.data() + pos, end - pos};
            pos = end + 2;
            std::size_t dpos = 0;
            while (dpos < record.size()) {
                const auto nl = record.find('\n', dpos);
                const auto line = record.substr(
                    dpos, nl == std::string_view::npos
                              ? std::string_view::npos
                              : nl - dpos);
                if (line.starts_with("data: ")) {
                    on_event(line.substr(6));
                } else if (line.starts_with("data:")) {
                    on_event(line.substr(5));
                }
                if (nl == std::string_view::npos) break;
                dpos = nl + 1;
            }
        }
    }

private:
    std::string buf_;
};

// ---------------------------------------------------------------------------
// Dispatch one OpenAI SSE payload. Shapes:
//   text:      {"choices":[{"delta":{"content":"..."},"index":0}]}
//   tool call: {"choices":[{"delta":{"tool_calls":[
//                {"index":N,"id":"...","function":{"name":"...",
//                                                  "arguments":"..."}}]}}]}
//   usage:     {"usage":{"prompt_tokens":N,"completion_tokens":N}}
//   done:      literal "[DONE]" payload — caller should ignore.
// Tool-call ids + names arrive on the first chunk for a given index and
// may be absent on subsequent chunks for that index; we track them by
// index so the ProviderDelta carries the id/name on every partial.
// ---------------------------------------------------------------------------
void dispatch_event(std::string_view payload,
                    std::vector<ProviderDelta>& out,
                    std::unordered_map<int, std::pair<std::string, std::string>>& tool_ctx) {
    if (payload == "[DONE]") return;
    auto j = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return;

    if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
        const auto& choice = j["choices"][0];
        const auto& delta  = choice.value("delta", nlohmann::json::object());

        if (delta.contains("content") && delta["content"].is_string()) {
            const auto text = delta.value("content", std::string{});
            if (!text.empty()) {
                ProviderDelta d;
                d.kind = DeltaKind::TextFragment;
                d.text = text;
                out.push_back(std::move(d));
            }
        }

        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
            for (const auto& tc : delta["tool_calls"]) {
                const auto index = tc.value("index", 0);
                if (tc.contains("id") && tc["id"].is_string()) {
                    tool_ctx[index].first = tc.value("id", std::string{});
                }
                const auto& func = tc.value("function", nlohmann::json::object());
                if (func.contains("name") && func["name"].is_string()) {
                    tool_ctx[index].second = func.value("name", std::string{});
                }
                if (func.contains("arguments") && func["arguments"].is_string()) {
                    const auto args = func.value("arguments", std::string{});
                    ProviderDelta d;
                    d.kind = DeltaKind::ToolCallPartial;
                    d.text = args;
                    d.tool_call_id   = tool_ctx[index].first;
                    d.tool_call_name = tool_ctx[index].second;
                    out.push_back(std::move(d));
                }
            }
        }
    }

    if (j.contains("usage") && j["usage"].is_object()) {
        const auto& usage = j["usage"];
        ProviderDelta d;
        d.kind          = DeltaKind::Usage;
        d.input_tokens  = usage.value("prompt_tokens",     std::int32_t{0});
        d.output_tokens = usage.value("completion_tokens", std::int32_t{0});
        out.push_back(std::move(d));
    }
}

} // namespace

OpenAIStreamingProvider::OpenAIStreamingProvider(OpenAIStreamingConfig cfg)
    : cfg_{std::move(cfg)},
      resolved_key_{resolve_api_key(cfg_)} {}

auto OpenAIStreamingProvider::execute_stream(const std::string& model,
                                              const std::string& prompt,
                                              int timeout_ms)
    -> std::generator<runtime::ProviderDelta> {
    std::vector<runtime::ProviderDelta> buffered;

    if (resolved_key_.empty()) {
        co_return;
    }

    httplib::Client client{cfg_.base_url.empty() ? "https://api.openai.com"
                                                  : cfg_.base_url};
    const auto t = std::chrono::milliseconds{timeout_ms > 0 ? timeout_ms
                                                            : cfg_.timeout.count()};
    client.set_read_timeout(std::chrono::duration_cast<std::chrono::seconds>(t));
    client.set_connection_timeout(
        std::chrono::duration_cast<std::chrono::seconds>(t));

    httplib::Headers headers{
        {"Authorization", "Bearer " + resolved_key_},
        {"accept",        "text/event-stream"},
    };

    SseParser parser;
    std::unordered_map<int, std::pair<std::string, std::string>> tool_ctx;
    const auto body = build_request_body(cfg_, model, prompt);

    auto result = client.Post(
        "/v1/chat/completions",
        headers,
        body,
        "application/json",
        [&](const char* data, std::size_t length) {
            parser.feed(std::string_view{data, length},
                        [&](std::string_view payload) {
                            dispatch_event(payload, buffered, tool_ctx);
                        });
            return true;
        });

    (void)result;  // Non-2xx or network failure: yield whatever buffered.

    for (auto& d : buffered) {
        co_yield std::move(d);
    }
}

} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
