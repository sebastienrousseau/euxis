#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "adapter.hpp"
#include "types.hpp"

namespace euxis::adapters {

class DiscordAdapter {
public:
    explicit DiscordAdapter(DiscordAdapterConfig config,
                            MessageHandler on_message = {});

    void connect();
    void receive(const nlohmann::json& message);
    void send(const std::string& text, const std::string& session_id);
    void ack(const std::string& message_id);
    void disconnect();

private:
    DiscordAdapterConfig config_;
    MessageHandler on_message_;
    std::mutex mutex_;
    std::unordered_map<std::string, nlohmann::json> session_meta_;
};

static_assert(Adapter<DiscordAdapter>);

} // namespace euxis::adapters
