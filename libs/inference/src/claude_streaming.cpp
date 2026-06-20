/// @file
/// @brief Anthropic Claude streaming provider implementation.

#include "euxis/inference/claude_streaming.hpp"

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

[[nodiscard]] auto resolve_api_key(const ClaudeStreamingConfig& cfg) -> std::string {
    if (!cfg.api_key.empty()) return cfg.api_key;
    if (!cfg.env_var_name.empty()) {
        if (const char* env = std::getenv(cfg.env_var_name.c_str())) {
            return std::string{env};
        }
    }
    return {};
}

[[nodiscard]] auto build_request_body(const ClaudeStreamingConfig& cfg,
                                     const std::string& model,
                                     const std::string& prompt) -> std::string {
    nlohmann::json body;
    body["model"]      = model.empty() ? cfg.model : model;
    body["max_tokens"] = cfg.max_tokens;
    body["stream"]     = true;
    body["messages"]   = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}},
    });
    return body.dump();
}

// Split `${base_url}` into scheme + host:port for httplib::Client.
// Returns {host_url, path_prefix_unused}. Path prefix is left for the
// caller — Anthropic's endpoint is always at `/v1/messages` against
// the host base.
[[nodiscard]] auto host_url_of(const std::string& base_url) -> std::string {
    // httplib::Client accepts `scheme://host:port` directly.
    return base_url.empty() ? "https://api.anthropic.com" : base_url;
}

// ---------------------------------------------------------------------------
// SSE event parser.
//
// Anthropic streams `event: <name>\ndata: <json>\n\n` records. This
// helper accumulates incoming bytes into a buffer, peels off complete
// records on `\n\n`, parses each one, and dispatches the JSON payload
// to the provided handler.
//
// Multi-line `data:` records are theoretically valid in the SSE spec
// but Anthropic never emits them, so we handle the common
// single-`data:`-line case directly.
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
            // Find the data: line within the record.
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
// Dispatch a single SSE event payload (the JSON after `data: `) into
// zero or more ProviderDelta records. Anthropic event types:
//   - message_start         : ignored
//   - content_block_start   : record tool_use id+name for later partials
//   - content_block_delta   : text_delta -> TextFragment;
//                             input_json_delta -> ToolCallPartial
//   - content_block_stop    : ignored
//   - message_delta         : usage -> Usage
//   - message_stop          : Done sentinel (optional — natural EOR works too)
// ---------------------------------------------------------------------------
struct ToolCallContext {
    std::string id;
    std::string name;
};

void dispatch_event(std::string_view payload,
                    std::vector<ProviderDelta>& out,
                    std::vector<ToolCallContext>& tool_ctx) {
    auto j = nlohmann::json::parse(std::string{payload}, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return;

    const auto type = j.value("type", std::string{});

    if (type == "content_block_start") {
        const auto index = j.value("index", 0);
        const auto& block = j.value("content_block", nlohmann::json::object());
        const auto block_type = block.value("type", std::string{});
        if (block_type == "tool_use") {
            const auto need = static_cast<std::size_t>(index) + 1;
            if (tool_ctx.size() < need) tool_ctx.resize(need);
            tool_ctx[index].id   = block.value("id", std::string{});
            tool_ctx[index].name = block.value("name", std::string{});
        }
        return;
    }

    if (type == "content_block_delta") {
        const auto index = j.value("index", 0);
        const auto& delta = j.value("delta", nlohmann::json::object());
        const auto delta_type = delta.value("type", std::string{});
        if (delta_type == "text_delta") {
            ProviderDelta d;
            d.kind = DeltaKind::TextFragment;
            d.text = delta.value("text", std::string{});
            out.push_back(std::move(d));
        } else if (delta_type == "input_json_delta") {
            ProviderDelta d;
            d.kind = DeltaKind::ToolCallPartial;
            d.text = delta.value("partial_json", std::string{});
            if (static_cast<std::size_t>(index) < tool_ctx.size()) {
                d.tool_call_id   = tool_ctx[index].id;
                d.tool_call_name = tool_ctx[index].name;
            }
            out.push_back(std::move(d));
        }
        return;
    }

    if (type == "message_delta") {
        const auto& usage = j.value("usage", nlohmann::json::object());
        if (usage.contains("input_tokens") || usage.contains("output_tokens")) {
            ProviderDelta d;
            d.kind          = DeltaKind::Usage;
            d.input_tokens  = usage.value("input_tokens",  std::int32_t{0});
            d.output_tokens = usage.value("output_tokens", std::int32_t{0});
            out.push_back(std::move(d));
        }
        return;
    }
    // message_start / content_block_stop / message_stop : nothing to emit.
}

} // namespace

ClaudeStreamingProvider::ClaudeStreamingProvider(ClaudeStreamingConfig cfg)
    : cfg_{std::move(cfg)},
      resolved_key_{resolve_api_key(cfg_)} {}

auto ClaudeStreamingProvider::execute_stream(const std::string& model,
                                             const std::string& prompt,
                                             int timeout_ms)
    -> std::generator<runtime::ProviderDelta> {
    std::vector<runtime::ProviderDelta> buffered;

    if (resolved_key_.empty()) {
        // No credential — yield nothing and let the consumer see an
        // empty stream. Real callers should check config().api_key
        // before calling. The IStreamingProvider contract does not
        // carry an error channel; a future revision can add a
        // DeltaKind::Error variant if needed.
        co_return;
    }

    httplib::Client client{host_url_of(cfg_.base_url)};
    const auto t = std::chrono::milliseconds{timeout_ms > 0 ? timeout_ms
                                                            : cfg_.timeout.count()};
    client.set_read_timeout(std::chrono::duration_cast<std::chrono::seconds>(t));
    client.set_connection_timeout(
        std::chrono::duration_cast<std::chrono::seconds>(t));

    httplib::Headers headers{
        {"x-api-key",         resolved_key_},
        {"anthropic-version", "2023-06-01"},
        {"accept",            "text/event-stream"},
    };

    SseParser parser;
    std::vector<ToolCallContext> tool_ctx;

    const auto body = build_request_body(cfg_, model, prompt);

    // POST and parse the SSE response at end-of-stream.
    // cpp-httplib 0.18.7 has no Post(... ContentReceiver) overload, so
    // true progressive streaming is not available here; the response is
    // buffered fully then dispatched event-by-event. Acceptable for
    // SSE bodies that are emitted entirely before the server closes
    // the connection; consumers see all events at the end.
    auto result = client.Post("/v1/messages", headers, body, "application/json");
    if (result && result->status >= 200 && result->status < 300) {
        parser.feed(result->body,
                    [&](std::string_view payload) {
                        dispatch_event(payload, buffered, tool_ctx);
                    });
    }

    if (!result || result->status < 200 || result->status >= 300) {
        // Network failure or non-2xx — emit whatever we have and stop.
        // The consumer will see a truncated stream; consistent with
        // the SSE-disconnect semantics the Anthropic API exposes.
    }

    for (auto& d : buffered) {
        co_yield std::move(d);
    }
}

} // namespace euxis::inference

#endif // EUXIS_HAS_STD_GENERATOR
