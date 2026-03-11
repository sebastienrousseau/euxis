#include <euxis/a2a/websocket_transport.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>

namespace euxis::a2a {

WebSocketA2ATransport::WebSocketA2ATransport(const std::string& url) 
    : client_(url) {
    
    client_.set_on_message([this](const std::string& raw_data) {
        try {
            auto j = nlohmann::json::from_msgpack(
                std::vector<uint8_t>(raw_data.begin(), raw_data.end()));
            
            std::string correlation_id = j.value("correlation_id", "");
            
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_responses_.contains(correlation_id)) {
                pending_responses_[correlation_id] = A2AMessage{}; 
                cv_.notify_all();
            }
        } catch (const std::exception& e) {
            spdlog::error("A2A WebSocket parsing error: {}", e.what());
        }
    });
    
    client_.connect();
}

auto WebSocketA2ATransport::send(const A2AMessage& msg) -> std::expected<A2AMessage, TransportError> {
    std::string correlation_id = "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    nlohmann::json j = msg.to_json();
    j["correlation_id"] = correlation_id;
    
    std::vector<uint8_t> packed = nlohmann::json::to_msgpack(j);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_responses_[correlation_id] = std::unexpected(TransportError::Timeout);
    }

    client_.send(std::string(packed.begin(), packed.end()));

    std::unique_lock<std::mutex> lock(mutex_);
    bool signaled = cv_.wait_for(lock, std::chrono::seconds(10), [this, &correlation_id] {
        return pending_responses_[correlation_id].has_value() || 
               pending_responses_[correlation_id].error() != TransportError::Timeout;
    });

    if (!signaled) {
        return std::unexpected(TransportError::Timeout);
    }

    auto result = std::move(pending_responses_[correlation_id]);
    pending_responses_.erase(correlation_id);
    return result;
}

} // namespace euxis::a2a
