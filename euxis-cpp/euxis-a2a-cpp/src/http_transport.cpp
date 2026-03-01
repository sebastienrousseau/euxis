#include "euxis/a2a/http_transport.hpp"

#include <httplib.h>
#include <spdlog/spdlog.h>

namespace euxis::a2a {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
HttpA2ATransport::HttpA2ATransport(std::string base_url)
    : base_url_(std::move(base_url)) {
    // Strip trailing slash for consistent URL construction
    if (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
    spdlog::debug("HttpA2ATransport configured with base_url: {}", base_url_);
}

// ---------------------------------------------------------------------------
// Helper: parse a URL into scheme+host and path components.
// Returns {scheme_host, path_prefix} e.g. {"http://localhost:8080", "/api"}
// ---------------------------------------------------------------------------
namespace {

struct ParsedUrl {
    std::string scheme_host;  // e.g. "http://localhost:8080"
    std::string path;         // e.g. "/api" or ""
};

auto parse_url(const std::string& url) -> ParsedUrl {
    ParsedUrl result;

    // Find the scheme separator
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        // No scheme — treat as host:port/path
        auto slash_pos = url.find('/');
        if (slash_pos == std::string::npos) {
            result.scheme_host = "http://" + url;
            result.path = "";
        } else {
            result.scheme_host = "http://" + url.substr(0, slash_pos);
            result.path = url.substr(slash_pos);
        }
        return result;
    }

    // Find the first slash after "scheme://host"
    auto host_start = scheme_end + 3;
    auto path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        result.scheme_host = url;
        result.path = "";
    } else {
        result.scheme_host = url.substr(0, path_start);
        result.path = url.substr(path_start);
    }

    // Strip trailing slash from path
    if (!result.path.empty() && result.path.back() == '/') {
        result.path.pop_back();
    }

    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// send  — POST a JSON-RPC payload to the base URL
// ---------------------------------------------------------------------------
auto HttpA2ATransport::send(std::string_view method,
                             const nlohmann::json& params)
    -> std::expected<nlohmann::json, std::string> {

    auto [scheme_host, path] = parse_url(base_url_);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(10);
    client.set_read_timeout(30);

    nlohmann::json rpc_request = {
        {"jsonrpc", "2.0"},
        {"method", std::string(method)},
        {"params", params},
        {"id", 1}
    };

    const auto body = rpc_request.dump();
    const std::string endpoint = path.empty() ? "/" : path;

    spdlog::debug("HttpA2ATransport::send POST {} -> {}", scheme_host, endpoint);
    auto res = client.Post(endpoint, body, "application/json");

    if (!res) {
        auto err = httplib::to_string(res.error());
        spdlog::error("HttpA2ATransport::send failed: {}", err);
        return std::unexpected("HTTP request failed: " + err);
    }

    if (res->status < 200 || res->status >= 300) {
        auto msg = std::format("HTTP error: status {}", res->status);
        spdlog::error("HttpA2ATransport::send: {}", msg);
        return std::unexpected(msg);
    }

    try {
        auto response_json = nlohmann::json::parse(res->body);
        return response_json;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("HttpA2ATransport::send: JSON parse error: {}", e.what());
        return std::unexpected(std::string("JSON parse error: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// discover  — GET {url}/.well-known/agent.json
// ---------------------------------------------------------------------------
auto HttpA2ATransport::discover(std::string_view url)
    -> std::expected<AgentCard, std::string> {

    auto [scheme_host, path] = parse_url(std::string(url));

    httplib::Client client(scheme_host);
    client.set_connection_timeout(10);
    client.set_read_timeout(30);

    const std::string endpoint = path + "/.well-known/agent.json";

    spdlog::debug("HttpA2ATransport::discover GET {} -> {}", scheme_host, endpoint);
    auto res = client.Get(endpoint);

    if (!res) {
        auto err = httplib::to_string(res.error());
        spdlog::error("HttpA2ATransport::discover failed: {}", err);
        return std::unexpected("HTTP request failed: " + err);
    }

    if (res->status < 200 || res->status >= 300) {
        auto msg = std::format("HTTP error: status {}", res->status);
        spdlog::error("HttpA2ATransport::discover: {}", msg);
        return std::unexpected(msg);
    }

    try {
        auto j = nlohmann::json::parse(res->body);
        return AgentCard::from_json(j);
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("HttpA2ATransport::discover: JSON parse error: {}", e.what());
        return std::unexpected(std::string("JSON parse error: ") + e.what());
    }
}

} // namespace euxis::a2a
