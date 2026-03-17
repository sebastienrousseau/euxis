#include "euxis/adapters/telegram.hpp"

#include <chrono>
#include <thread>

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
    // Long-polling mode: spawn a background thread calling getUpdates
    spdlog::info("Telegram adapter starting long-poll loop");
    poll_thread_ = std::jthread([this](std::stop_token stoken) {
        while (!stoken.stop_requested() && !stop_.load(std::memory_order_relaxed)) {
            auto resp = api_call("getUpdates",
                                 {{"offset", offset_},
                                  {"timeout", config_.poll_timeout}});
            if (resp.contains("result") && resp["result"].is_array()) {
                for (const auto& update : resp["result"]) {
                    int64_t update_id = update.value("update_id", int64_t{0});
                    if (update_id >= offset_) {
                        offset_ = static_cast<int>(update_id + 1);
                    }
                    receive(update);
                }
            }
            // Brief sleep between poll cycles
            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    static_cast<int>(config_.poll_interval * 1000)));
        }
    });
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
    stop_.store(true, std::memory_order_relaxed);
    if (poll_thread_.joinable()) {
        poll_thread_.request_stop();
        poll_thread_.join();
    }
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
    try { return std::stoll(raw); } catch (const std::exception&) { return std::nullopt; }
}

auto TelegramAdapter::api_call(const std::string& method,
                               const nlohmann::json& data) -> nlohmann::json {
    httplib::Client cli(!config_.api_base_url.empty() ? config_.api_base_url : "https://api.telegram.org");
    cli.set_connection_timeout(15);
    auto path = "/bot" + config_.token + "/" + method;
    auto res = cli.Post(path, data.dump(), "application/json");
    if (res && !res->body.empty()) {
        try { return nlohmann::json::parse(res->body); } catch (const std::exception&) {}
    }
    return {};
}

} // namespace euxis::adapters
