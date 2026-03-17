#include "euxis/adapters/registry.hpp"

namespace euxis::adapters {

auto build_adapters(const nlohmann::json& config, MessageHandler on_message)
    -> std::unordered_map<std::string, AnyAdapter> {
    if (!config.is_object()) {
        return {};
    }
    auto channels =
        config.value("gateway", nlohmann::json::object())
            .value("channels", nlohmann::json::object());

    std::unordered_map<std::string, AnyAdapter> adapters;

    if (auto slack = channels.value("slack", nlohmann::json::object());
        slack.value("enabled", false)) {
        adapters.emplace(std::string("slack"),
                         AnyAdapter(std::make_unique<SlackAdapter>(
                             SlackAdapterConfig{
                                 .token = slack.value("token", ""),
                                 .app_token = slack.value("app_token", ""),
                                 .mode = slack.value("mode", "socket"),
                                 .api_base_url = slack.value("api_base_url", "")},
                             on_message)));
    }

    if (auto tg = channels.value("telegram", nlohmann::json::object());
        tg.value("enabled", false)) {
        adapters.emplace(std::string("telegram"),
                         AnyAdapter(std::make_unique<TelegramAdapter>(
                             TelegramAdapterConfig{
                                 .token = tg.value("token", ""),
                                 .mode = tg.value("mode", "webhook"),
                                 .webhook_url = tg.value("webhook_url", ""),
                                 .poll_timeout = tg.value("poll_timeout", 20),
                                 .poll_interval = tg.value("poll_interval", 1.5),
                                 .api_base_url = tg.value("api_base_url", "")},
                             on_message)));
    }

    if (auto dc = channels.value("discord", nlohmann::json::object());
        dc.value("enabled", false)) {
        adapters.emplace(std::string("discord"),
                         AnyAdapter(std::make_unique<DiscordAdapter>(
                             DiscordAdapterConfig{
                                 .token = dc.value("token", ""),
                                 .enabled = true,
                                 .api_base_url = dc.value("api_base_url", "")},
                             on_message)));
    }

    if (auto wa = channels.value("whatsapp", nlohmann::json::object());
        wa.value("enabled", false)) {
        adapters.emplace(std::string("whatsapp"),
                         AnyAdapter(std::make_unique<WhatsAppAdapter>(
                             WhatsAppAdapterConfig{
                                 .token = wa.value("token", ""),
                                 .phone_number_id = wa.value("phone_number_id", ""),
                                 .verify_token = wa.value("verify_token", ""),
                                 .enabled = true,
                                 .api_base_url = wa.value("api_base_url", "")},
                             on_message)));
    }

    return adapters;
}

} // namespace euxis::adapters
