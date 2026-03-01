#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "adapter.hpp"
#include "types.hpp"

namespace euxis::adapters {

class WhatsAppAdapter {
public:
    explicit WhatsAppAdapter(WhatsAppAdapterConfig config,
                             MessageHandler on_message = {});

    void connect();
    void receive(const nlohmann::json& payload);
    void send(const std::string& text, const std::string& session_id);
    void ack(const std::string& message_id);
    void disconnect();

private:
    WhatsAppAdapterConfig config_;
    MessageHandler on_message_;
    std::mutex mutex_;
    std::unordered_map<std::string, nlohmann::json> session_meta_;

    void handle_single_message(const nlohmann::json& message,
                               const nlohmann::json& value);
};

static_assert(Adapter<WhatsAppAdapter>);

} // namespace euxis::adapters
