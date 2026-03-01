#include "euxis/adapters/telegram.hpp"

#include <spdlog/spdlog.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

namespace euxis::adapters {

TelegramAdapter::TelegramAdapter(TelegramAdapterConfig config,
                                 MessageHandler on_message)
    : config_(std::move(config)), on_message_(std::move(on_message)) {}

void TelegramAdapter::connect() {
    if (config_.token.empty()) {
        spdlog::warn("Telegram adapter missing token");
        return;
    }
    if (config_.mode == "webhook") {
        if (config_.webhook_url.empty()) {
            spdlog::warn("Telegram webhook_url is empty");
            return;
        }
        api_call("setWebhook", {{"url", config_.webhook_url}});
        return;
    }
    // Polling mode not implemented in C++ MVP (requires background HTTP loop)
    spdlog::info("Telegram adapter ready (webhook mode only in C++)");
}

void TelegramAdapter::receive(const nlohmann::json& message) {
    auto payload = message.contains("message") ? message["message"] : message;
    if (!payload.is_object()) return;

    auto chat = payload.value("chat", nlohmann::json::object());
    auto chat_id = chat.contains("id") ? chat["id"].get<int64_t>() : 0;
    if (chat_id == 0) return;

    auto text = payload.value("text", payload.value("caption", ""));
    auto chat_type = chat.value("type", "");
    auto scope = (chat_type == "group" || chat_type == "supergroup")
                     ? "group" : "dm";
    auto session_id = "telegram_" + std::to_string(chat_id);

    session_meta_[session_id] = {{"chat_id", chat_id}};

    if (on_message_) {
        on_message_(session_id, text,
                    {{"channel_id", "telegram"},
                     {"chat_id", chat_id},
                     {"scope", scope}});
    }
}

void TelegramAdapter::send(const std::string& text,
                           const std::string& session_id) {
    if (config_.token.empty()) return;
    auto chat_id = resolve_chat_id(session_id);
    if (!chat_id) return;
    api_call("sendMessage",
             {{"chat_id", *chat_id}, {"text", text}});
}

void TelegramAdapter::ack(const std::string& /*message_id*/) {}

void TelegramAdapter::disconnect() {
    stop_.store(true);
    if (config_.mode == "webhook" && !config_.token.empty()) {
        api_call("deleteWebhook", {});
    }
}

auto TelegramAdapter::resolve_chat_id(const std::string& session_id)
    -> std::optional<int64_t> {
    auto it = session_meta_.find(session_id);
    if (it != session_meta_.end() && it->second.contains("chat_id"))
        return it->second["chat_id"].get<int64_t>();

    auto raw = session_id;
    if (raw.starts_with("telegram_")) raw = raw.substr(9);
    try { return std::stoll(raw); } catch (...) { return std::nullopt; }
}

auto TelegramAdapter::api_call(const std::string& method,
                               const nlohmann::json& data) -> nlohmann::json {
    httplib::SSLClient cli("api.telegram.org");
    cli.set_connection_timeout(15);
    auto path = "/bot" + config_.token + "/" + method;
    auto res = cli.Post(path, data.dump(), "application/json");
    if (res && !res->body.empty()) {
        try { return nlohmann::json::parse(res->body); } catch (...) {}
    }
    return {};
}

} // namespace euxis::adapters
