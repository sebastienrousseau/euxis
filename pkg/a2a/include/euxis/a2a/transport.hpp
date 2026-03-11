#pragma once

#include <expected>
#include <memory>
#include <string>

#include "message.hpp"

namespace euxis::a2a {

/**
 * @brief Error types for A2A transport layer operations.
 */
enum class TransportError {
    ConnectionFailed,   ///< Unable to establish network connection.
    Timeout,            ///< Target did not respond in time.
    ProtocolError,      ///< Invalid message format or unexpected response.
    AuthFailed,         ///< Invalid or missing credentials.
};

/**
 * @brief Base interface for A2A communication channels.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Send an A2A message and wait for the response.
     * @param msg The message to send.
     * @return std::expected<Message, TransportError> The reply or error.
     */
    virtual auto send(const Message& msg) -> std::expected<Message, TransportError> = 0;
};

} // namespace euxis::a2a
