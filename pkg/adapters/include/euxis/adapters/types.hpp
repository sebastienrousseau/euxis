/// @file
/// @brief Types for adapters
#pragma once

#include <string>

namespace euxis::adapters {

struct AdapterConfig {
    bool enabled{false};
    std::string mode;
};

struct SlackAdapterConfig {
    std::string token;
    std::string app_token;
    std::string mode{"socket"};
    std::string api_base_url; // Empty = default
};

struct TelegramAdapterConfig {
    std::string token;
    std::string mode{"webhook"};
    std::string webhook_url;
    int poll_timeout{20};
    double poll_interval{1.5};
    std::string api_base_url; // Empty = default
};

struct DiscordAdapterConfig {
    std::string token;
    bool enabled{false};
    std::string api_base_url; // Empty = default
};

struct WhatsAppAdapterConfig {
    std::string token;
    std::string phone_number_id;
    std::string verify_token;
    bool enabled{false};
    std::string api_base_url; // Empty = default
};

} // namespace euxis::adapters
