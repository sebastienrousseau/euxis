#include "euxis/gateway/routes.hpp"
#include "euxis/gateway/state.hpp"

#include <nlohmann/json.hpp>

namespace euxis::gateway {

void register_session_routes(httplib::Server& server, const RouteContext& ctx) {
    server.Get(R"(/api/sessions/(\w+))",
               [ctx](const httplib::Request& req, httplib::Response& res) {
                   if (!authorize_request(req, res, ctx)) return;
                   auto session_id = req.matches[1].str();
                   auto history = load_session_from_disk(session_id);
                   nlohmann::json body = {
                       {"session_id", session_id},
                       {"messages", history},
                   };
                   res.set_content(body.dump(), "application/json");
               });

    server.Post("/api/sessions",
                [ctx](const httplib::Request& req, httplib::Response& res) {
                    if (!authorize_request(req, res, ctx)) return;
                    try {
                        auto j = nlohmann::json::parse(req.body);
                        auto session_id = j.value("session_id", "");
                        auto content = j.value("content", "");
                        if (session_id.empty() || content.empty()) {
                            res.status = 400;
                            res.set_content(
                                R"({"error":"session_id and content required"})",
                                "application/json");
                            return;
                        }
                        nlohmann::json entry = {
                            {"role", "user"},
                            {"content", content},
                            {"timestamp", timestamp()},
                        };
                        persist_message(session_id, entry);
                        audit_log({{"event", "session.message"},
                                   {"session_id", session_id},
                                   {"timestamp", timestamp()}});
                        res.set_content(entry.dump(), "application/json");
                    } catch (const std::exception&) {
                        res.status = 400;
                        res.set_content(R"({"error":"invalid json"})",
                                        "application/json");
                    }
                });
}

} // namespace euxis::gateway
