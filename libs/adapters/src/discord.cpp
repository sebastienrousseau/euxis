#include "euxis/adapters/discord.hpp"

// httplib.h triggers GCC's -Wmaybe-uninitialized. Clang has no such
// flag and -Wunknown-warning-option is -Werror under AppleClang.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <httplib.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

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
    if (config_.token.empty()) {
        spdlog::warn("Discord adapter: cannot send without token");
        return;
    }

    // Resolve channel_id from session metadata
    std::string channel_id;
    {
        std::lock_guard lock(mutex_);
        auto it = session_meta_.find(session_id);
        if (it != session_meta_.end() && it->second.contains("channel_id")) {
            channel_id = it->second["channel_id"].get<std::string>();
        }
    }

    if (channel_id.empty()) {
        // Try extracting from session_id format "discord_{channel_id}"
        if (session_id.starts_with("discord_")) {
            channel_id = session_id.substr(8);
        }
    }

    if (channel_id.empty()) {
        spdlog::warn("Discord adapter: no channel_id for session '{}'", session_id);
        return;
    }

    // POST to Discord REST API
    httplib::Client cli(!config_.api_base_url.empty() ? config_.api_base_url : "https://discord.com");
    cli.set_connection_timeout(10);

    httplib::Headers headers = {
        {"Authorization", "Bot " + config_.token},
    };

    nlohmann::json body = {{"content", text}};
    auto path = "/api/v10/channels/" + channel_id + "/messages";
    auto res = cli.Post(path, headers, body.dump(), "application/json");

    if (!res) {
        spdlog::warn("Discord API request failed for channel {}", channel_id);
    } else if (res->status != 200) {
        spdlog::warn("Discord API returned HTTP {} for channel {}",
                      res->status, channel_id);
    }
}

void DiscordAdapter::ack(const std::string& /*message_id*/) {}

void DiscordAdapter::disconnect() {}

} // namespace euxis::adapters
