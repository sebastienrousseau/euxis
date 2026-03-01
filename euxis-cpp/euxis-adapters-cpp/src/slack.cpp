#include "euxis/adapters/slack.hpp"

#include <spdlog/spdlog.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

namespace euxis::adapters {

SlackAdapter::SlackAdapter(SlackAdapterConfig config, MessageHandler on_message)
    : config_(std::move(config)), on_message_(std::move(on_message)) {}

void SlackAdapter::connect() {
    if (config_.mode != "socket") return;
    if (config_.token.empty() || config_.app_token.empty()) {
        spdlog::warn("Slack adapter missing token/app_token for socket mode");
        return;
    }
    // Socket mode requires slack_sdk — not available in C++.
    // Webhook-based receive() still works.
    spdlog::info("Slack adapter ready (webhook-based receive)");
}

void SlackAdapter::receive(const nlohmann::json& message) {
    auto payload = message.contains("event") ? message["event"] : message;
    if (!payload.is_object()) return;
    if (payload.value("subtype", "") == "bot_message") return;

    auto channel = payload.value("channel",
                       payload.value("channel_id", "unknown"));
    auto text = payload.value("text", payload.value("message", ""));
    auto thread_ts = payload.value("thread_ts", "");
    auto user = payload.value("user", payload.value("user_id", ""));
    auto channel_type = payload.value("channel_type", "");
    auto scope = (channel_type == "channel" || channel_type == "group" ||
                  channel_type == "mpim")
                     ? "group"
                     : "dm";

    std::string session_id = "slack_" + channel;
    if (!thread_ts.empty()) session_id += ":" + thread_ts;

    {
        std::lock_guard lock(mutex_);
        session_meta_[session_id] = {
            {"channel", channel}, {"thread_ts", thread_ts}, {"user", user}};
    }

    if (on_message_) {
        on_message_(session_id, text,
                    {{"channel_id", channel}, {"scope", scope}});
    }
}

void SlackAdapter::send(const std::string& text,
                        const std::string& session_id) {
    if (config_.token.empty()) {
        spdlog::warn("Slack adapter missing token for send");
        return;
    }
    auto [channel, thread_ts] = resolve_session(session_id);
    if (channel.empty()) return;

    httplib::SSLClient cli("slack.com");
    cli.set_connection_timeout(10);
    nlohmann::json payload = {{"channel", channel}, {"text", text}};
    if (!thread_ts.empty()) payload["thread_ts"] = thread_ts;

    httplib::Headers headers = {
        {"Authorization", "Bearer " + config_.token},
        {"Content-Type", "application/json; charset=utf-8"},
    };
    auto res = cli.Post("/api/chat.postMessage", headers,
                        payload.dump(), "application/json");
    if (!res || res->status != 200) {
        spdlog::warn("Slack send failed");
    }
}

void SlackAdapter::ack(const std::string& /*message_id*/) {}

void SlackAdapter::disconnect() {}

auto SlackAdapter::resolve_session(const std::string& session_id)
    -> std::pair<std::string, std::string> {
    {
        std::lock_guard lock(mutex_);
        auto it = session_meta_.find(session_id);
        if (it != session_meta_.end()) {
            return {it->second.value("channel", ""),
                    it->second.value("thread_ts", "")};
        }
    }
    auto raw = session_id;
    if (raw.starts_with("slack_")) raw = raw.substr(6);
    auto colon = raw.find(':');
    if (colon != std::string::npos)
        return {raw.substr(0, colon), raw.substr(colon + 1)};
    return {raw, {}};
}

} // namespace euxis::adapters
