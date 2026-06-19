/// @file
/// @brief Google Gemini streaming provider implementation.

#include "euxis/inference/gemini_streaming.hpp"

#if defined(EUXIS_HAS_STD_GENERATOR)

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace euxis::inference {

namespace {

using runtime::DeltaKind;
using runtime::ProviderDelta;

[[nodiscard]] auto resolve_api_key(const GeminiStreamingConfig& cfg) -> std::string {
    if (!cfg.api_key.empty()) return cfg.api_key;
    if (!cfg.env_var_name.empty()) {
        if (const char* env = std::getenv(cfg.env_var_name.c_str())) {
            return std::string{env};
        }
    }
    return {};
}

[[nodiscard]] auto build_request_body(const std::string& prompt) -> std::string {
    nlohmann::json body;
    body["contents"] = nlohmann::json::array({
        {{"role", "user"},
         {"parts", nlohmann::json::array({{{"text", prompt}}})}},
    });
    return body.dump();
}

[[nodiscard]] auto build_path(const GeminiStreamingConfig& cfg,
                              const std::string& model,
                              const std::string& api_key) -> std::string {
    const auto m = model.empty() ? cfg.model : model;
    return "/v1beta/models/" + m + ":streamGenerateContent?alt=sse&key=" + api_key;
}

// ---------------------------------------------------------------------------
// SSE parser — identical shape to the other two providers.
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
// Dispatch one Gemini SSE payload. Shape:
//   {"candidates":[{"content":{"parts":[{"text":"..."} |
//                                       {"functionCall":{"name":"...",
//                                                        "args":{...}}}]}}],
//    "usageMetadata":{"promptTokenCount":N,"candidatesTokenCount":N}}
// Gemini emits functionCall as a fully-formed object — there is no
// partial-JSON streaming for tool args. We serialise it back out as a
// single ToolCallPartial whose text carries the full args JSON.
// ---------------------------------------------------------------------------
void dispatch_event(std::string_view payload,
                    std::vector<ProviderDelta>& out) {
    auto j = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return;

    if (j.contains("candidates") && j["candidates"].is_array() &&
        !j["candidates"].empty()) {
        const auto& cand    = j["candidates"][0];
        const auto& content = cand.value("content", nlohmann::json::object());
        const auto& parts   = content.value("parts", nlohmann::json::array());

        for (const auto& part : parts) {
            if (part.contains("text") && part["text"].is_string()) {
                ProviderDelta d;
                d.kind = DeltaKind::TextFragment;
                d.text = part.value("text", std::string{});
                if (!d.text.empty()) out.push_back(std::move(d));
            } else if (part.contains("functionCall") &&
                       part["functionCall"].is_object()) {
                const auto& fc = part["functionCall"];
                ProviderDelta d;
                d.kind           = DeltaKind::ToolCallPartial;
                d.tool_call_name = fc.value("name", std::string{});
                // Gemini doesn't issue tool-call ids; mint a stable
                // synthetic id from the name so downstream code that
                // groups partials by id still works.
                d.tool_call_id   = "gemini-fc-" + d.tool_call_name;
                d.text           = fc.value("args", nlohmann::json::object()).dump();
                out.push_back(std::move(d));
            }
        }
    }

    if (j.contains("usageMetadata") && j["usageMetadata"].is_object()) {
        const auto& usage = j["usageMetadata"];
        ProviderDelta d;
        d.kind          = DeltaKind::Usage;
        d.input_tokens  = usage.value("promptTokenCount",     std::int32_t{0});
        d.output_tokens = usage.value("candidatesTokenCount", std::int32_t{0});
        out.push_back(std::move(d));
    }
}

} // namespace

GeminiStreamingProvider::GeminiStreamingProvider(GeminiStreamingConfig cfg)
    : cfg_{std::move(cfg)},
      resolved_key_{resolve_api_key(cfg_)} {}

auto GeminiStreamingProvider::execute_stream(const std::string& model,
                                              const std::string& prompt,
                                              int timeout_ms)
    -> std::generator<runtime::ProviderDelta> {
    std::vector<runtime::ProviderDelta> buffered;

    if (resolved_key_.empty()) {
        co_return;
    }

    httplib::Client client{cfg_.base_url.empty()
                               ? "https://generativelanguage.googleapis.com"
                               : cfg_.base_url};
    const auto t = std::chrono::milliseconds{timeout_ms > 0 ? timeout_ms
                                                            : cfg_.timeout.count()};
    client.set_read_timeout(std::chrono::duration_cast<std::chrono::seconds>(t));
    client.set_connection_timeout(
        std::chrono::duration_cast<std::chrono::seconds>(t));

    httplib::Headers headers{
        {"accept", "text/event-stream"},
    };

    SseParser parser;
    const auto body = build_request_body(prompt);
    const auto path = build_path(cfg_, model, resolved_key_);

    // cpp-httplib 0.18.7 has no Post(... ContentReceiver) overload; buffer
    // the response and dispatch SSE events at end-of-stream.
    auto result = client.Post(path.c_str(), headers, body, "application/json");
    if (result && result->status >= 200 && result->status < 300) {
        parser.feed(result->body,
                    [&](std::string_view payload) {
                        dispatch_event(payload, buffered);
                    });
    }

    for (auto& d : buffered) {
        co_yield std::move(d);
    }
}

} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
