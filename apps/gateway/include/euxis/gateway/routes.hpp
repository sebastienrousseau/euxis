/// @file
/// @brief Gateway routes
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <httplib.h>
#pragma GCC diagnostic pop

namespace euxis::gateway {

void register_health_routes(httplib::Server& server);
void register_session_routes(httplib::Server& server);
void register_webhook_routes(httplib::Server& server);
void register_admin_routes(httplib::Server& server);
void register_mcp_routes(httplib::Server& server);

} // namespace euxis::gateway
