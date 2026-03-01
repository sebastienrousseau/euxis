#include "euxis/adapters/discord.hpp"

#include <spdlog/spdlog.h>

namespace euxis::adapters {

DiscordAdapter::DiscordAdapter(DiscordAdapterConfig config,
                               MessageHandler on_message)
    : config_(std::move(config)), on_message_(std::move(on_message)) {}

void DiscordAdapter::connect() {
    if (!config_.enabled || config_.token.empty()) {
        spdlog::info("Discord adapter disabled or missing token");
        return;
    }
    // discord.py equivalent requires a C++ Discord library.
    // Webhook-based receive() still works.
    spdlog::info("Discord adapter ready (webhook-based receive)");
}

void DiscordAdapter::receive(const nlohmann::json& message) {
    auto channel_id = message.value("channel_id", "");
    auto author_id = message.value("author_id", "");
    auto content = message.value("content", "");

    auto session_id = "discord_" + channel_id;
    auto scope = message.value("channel_type", "") == "private" ? "dm" : "group";

    {
        std::lock_guard lock(mutex_);
        session_meta_[session_id] = {{"channel_id", channel_id}};
    }

    if (on_message_) {
        on_message_(session_id, content,
                    {{"channel_id", channel_id},
                     {"sender", author_id},
                     {"scope", scope}});
    }
}

void DiscordAdapter::send(const std::string& text,
                          const std::string& session_id) {
    (void)text;
    (void)session_id;
    // Requires Discord bot WebSocket or REST API with bot token.
    // Not implemented in C++ MVP.
    spdlog::warn("Discord send not implemented in C++ adapter");
}

void DiscordAdapter::ack(const std::string& /*message_id*/) {}

void DiscordAdapter::disconnect() {}

} // namespace euxis::adapters
