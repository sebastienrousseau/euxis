#pragma once

#include <string>
#include <string_view>

#include "euxis/a2a/transport.hpp"

namespace euxis::a2a {

/// HTTP-based A2A transport using cpp-httplib.
///
/// - send() POSTs a JSON-RPC payload to the configured base URL.
/// - discover() fetches {url}/.well-known/agent.json and parses as AgentCard.
class HttpA2ATransport final : public A2ATransport {
public:
    explicit HttpA2ATransport(std::string base_url);

    [[nodiscard]] auto send(std::string_view method, const nlohmann::json& params)
        -> std::expected<nlohmann::json, std::string> override;

    [[nodiscard]] auto discover(std::string_view url)
        -> std::expected<AgentCard, std::string> override;

private:
    std::string base_url_;
};

} // namespace euxis::a2a
