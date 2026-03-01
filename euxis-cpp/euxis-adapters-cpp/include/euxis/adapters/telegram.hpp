#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "adapter.hpp"
#include "types.hpp"

namespace euxis::adapters {

class TelegramAdapter {
public:
    explicit TelegramAdapter(TelegramAdapterConfig config,
                             MessageHandler on_message = {});

    void connect();
    void receive(const nlohmann::json& message);
    void send(const std::string& text, const std::string& session_id);
    void ack(const std::string& message_id);
    void disconnect();

private:
    TelegramAdapterConfig config_;
    MessageHandler on_message_;
    std::unordered_map<std::string, nlohmann::json> session_meta_;
    std::atomic<bool> stop_{false};
    std::jthread poll_thread_;
    int offset_{0};

    auto resolve_chat_id(const std::string& session_id) -> std::optional<int64_t>;
    auto api_call(const std::string& method, const nlohmann::json& data)
        -> nlohmann::json;
};

static_assert(Adapter<TelegramAdapter>);

} // namespace euxis::adapters
