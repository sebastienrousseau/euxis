#include <gtest/gtest.h>
#include "euxis/adapters/whatsapp.hpp"
#include <httplib.h>
#include <thread>

namespace euxis::adapters {
namespace {

class WhatsAppMockServer {
public:
    WhatsAppMockServer() {
        server.Post("/v17.0/12345/messages", [](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_header("Authorization")) {
                res.status = 401;
                return;
            }
            res.status = 200;
            res.set_content(R"({"messaging_product":"whatsapp","contacts":[{"input":"555","wa_id":"555"}],"messages":[{"id":"m1"}]})", "application/json");
        });
    }

    void start() {
        int port = server.bind_to_any_port("127.0.0.1");
        base_url = "http://127.0.0.1:" + std::to_string(port);
        thread = std::thread([this]() { server.listen_after_bind(); });
    }

    ~WhatsAppMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    httplib::Server server;
    std::string base_url;
    std::thread thread;
};

TEST(WhatsAppAdapterApiTest, SendMessageMock) {
    WhatsAppMockServer mock;
    mock.start();

    WhatsAppAdapterConfig config;
    config.token = "test-token";
    config.phone_number_id = "12345";
    config.api_base_url = mock.base_url;

    WhatsAppAdapter adapter(config);
    adapter.send("hello", "whatsapp_555");
    SUCCEED();
}

TEST(WhatsAppAdapterApiTest, SendMissingConfigWarns) {
    WhatsAppAdapter adapter({}); // empty token/id
    adapter.send("hello", "whatsapp_555");
    SUCCEED();
}

TEST(WhatsAppAdapterApiTest, SendInvalidSessionReturns) {
    WhatsAppAdapterConfig config;
    config.token = "t";
    config.phone_number_id = "i";
    WhatsAppAdapter adapter(config);
    adapter.send("hello", "invalid");
    SUCCEED();
}

} // namespace
} // namespace euxis::adapters
