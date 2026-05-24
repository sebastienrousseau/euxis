/// @file
/// @brief Gateway routes
#pragma once

#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

#include <nlohmann/json.hpp>

namespace euxis::gateway {

/// Maximum allowed request body size (1 MB).
constexpr size_t kMaxRequestBodySize = 1ULL * 1024 * 1024;

/// Shared route context with auth token from config.
struct RouteContext {
    std::string auth_token;
};

/// Extract Bearer token from Authorization header.
inline std::string extract_bearer(const httplib::Request& req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) return {};
    const auto& val = it->second;
    if (val.starts_with("Bearer ")) return val.substr(7);
    return {};
}

/// Check auth and body size. Returns true if request is allowed, sets error response if not.
bool authorize_request(const httplib::Request& req, httplib::Response& res,
                       const RouteContext& ctx);

void register_health_routes(httplib::Server& server);
void register_session_routes(httplib::Server& server, const RouteContext& ctx);
void register_webhook_routes(httplib::Server& server, const RouteContext& ctx);
void register_admin_routes(httplib::Server& server, const RouteContext& ctx);
void register_mcp_routes(httplib::Server& server, const RouteContext& ctx);

} // namespace euxis::gateway
