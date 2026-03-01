#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include <nlohmann/json.hpp>

#include "adapter.hpp"
#include "discord.hpp"
#include "slack.hpp"
#include "telegram.hpp"
#include "whatsapp.hpp"

namespace euxis::adapters {

using AnyAdapter =
    std::variant<std::unique_ptr<SlackAdapter>, std::unique_ptr<TelegramAdapter>,
                 std::unique_ptr<DiscordAdapter>,
                 std::unique_ptr<WhatsAppAdapter>>;

/// Build adapters from gateway config.
[[nodiscard]] auto build_adapters(const nlohmann::json& config,
                                  MessageHandler on_message = {})
    -> std::unordered_map<std::string, AnyAdapter>;

} // namespace euxis::adapters
