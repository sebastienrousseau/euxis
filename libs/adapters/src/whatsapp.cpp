#include "euxis/adapters/whatsapp.hpp"

#include <spdlog/spdlog.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

namespace euxis::adapters {

WhatsAppAdapter::WhatsAppAdapter(WhatsAppAdapterConfig config,
                                 MessageHandler on_message)
    : config_(std::move(config)), on_message_(std::move(on_message)) {}

void WhatsAppAdapter::connect() {
    if (!config_.enabled || config_.token.empty()) {
        spdlog::info("WhatsApp adapter disabled or missing token");
        return;
    }
    spdlog::info("WhatsApp adapter ready (webhook-based)");
}

void WhatsAppAdapter::receive(const nlohmann::json& payload) {
    for (const auto& entry : payload.value("entry", nlohmann::json::array())) {
        for (const auto& change : entry.value("changes", nlohmann::json::array())) {
            auto value = change.value("value", nlohmann::json::object());
            for (const auto& msg : value.value("messages", nlohmann::json::array())) {
                handle_single_message(msg, value);
            }
        }
    }
}

void WhatsAppAdapter::handle_single_message(const nlohmann::json& message,
                                            const nlohmann::json& /*value*/) {
    auto from_id = message.value("from", "");
    auto text = message.contains("text")
                    ? message["text"].value("body", "")
                    : std::string{};
    if (text.empty() || from_id.empty()) return;

    auto session_id = "whatsapp_" + from_id;
    {
        std::lock_guard lock(mutex_);
        session_meta_[session_id] = {{"from_id", from_id}};
    }

    if (on_message_) {
        on_message_(session_id, text,
                    {{"channel_id", from_id},
                     {"sender", from_id},
                     {"scope", "dm"}});
    }
}

void WhatsAppAdapter::send(const std::string& text,
                           const std::string& session_id) {
    if (config_.token.empty() || config_.phone_number_id.empty()) {
        spdlog::warn("WhatsApp adapter missing token or phone_number_id");
        return;
    }

    std::string from_id;
    {
        std::lock_guard lock(mutex_);
        auto it = session_meta_.find(session_id);
        if (it != session_meta_.end())
            from_id = it->second.value("from_id", "");
    }
    if (from_id.empty()) {
        if (session_id.starts_with("whatsapp_"))
            from_id = session_id.substr(9);
        else return;
    }

    httplib::Client cli(!config_.api_base_url.empty() ? config_.api_base_url : "https://graph.facebook.com");
    cli.set_connection_timeout(10);
    nlohmann::json payload = {
        {"messaging_product", "whatsapp"},
        {"recipient_type", "individual"},
        {"to", from_id},
        {"type", "text"},
        {"text", {{"body", text}}},
    };
    auto path = "/v17.0/" + config_.phone_number_id + "/messages";
    httplib::Headers headers = {
        {"Authorization", "Bearer " + config_.token},
        {"Content-Type", "application/json"},
    };
    cli.Post(path, headers, payload.dump(), "application/json");
}

void WhatsAppAdapter::ack(const std::string& /*message_id*/) {}

void WhatsAppAdapter::disconnect() {}

} // namespace euxis::adapters
