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
    ConnectionFailed,
    Timeout,
    ProtocolError,
    AuthFailed,
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
     * @return std::expected<A2AMessage, TransportError> The reply or error.
     */
    virtual auto send(const A2AMessage& msg) -> std::expected<A2AMessage, TransportError> = 0;
};

} // namespace euxis::a2a
