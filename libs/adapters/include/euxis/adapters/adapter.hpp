#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::adapters {

/// Message handler callback: (session_id, text, meta).
using MessageHandler =
    std::function<void(const std::string&, const std::string&,
                       const nlohmann::json&)>;

/// C++20 concept defining the Adapter interface.
template <typename T>
concept Adapter = requires(T t, const nlohmann::json& msg,
                           const std::string& text,
                           const std::string& session_id,
                           const std::string& message_id) {
    { t.connect() } -> std::same_as<void>;
    { t.receive(msg) } -> std::same_as<void>;
    { t.send(text, session_id) } -> std::same_as<void>;
    { t.ack(message_id) } -> std::same_as<void>;
    { t.disconnect() } -> std::same_as<void>;
};

} // namespace euxis::adapters
