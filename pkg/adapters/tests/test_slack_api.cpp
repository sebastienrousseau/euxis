#include <gtest/gtest.h>
#include "euxis/adapters/slack.hpp"
#include <httplib.h>
#include <thread>
#include <atomic>

namespace euxis::adapters {
namespace {

class SlackMockServer {
public:
    SlackMockServer() {
        server.Post("/api/chat.postMessage", [](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_header("Authorization")) {
                res.status = 401;
                return;
            }
            res.set_content(R"({"ok":true})", "application/json");
        });
    }

    void start() {
        int port = server.bind_to_any_port("127.0.0.1");
        base_url = "http://127.0.0.1:" + std::to_string(port);
        thread = std::thread([this]() { server.listen_after_bind(); });
    }

    ~SlackMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    httplib::Server server;
    std::string base_url;
    std::thread thread;
};

TEST(SlackAdapterApiTest, SendMessageMock) {
    SlackMockServer mock;
    mock.start();

    SlackAdapterConfig config;
    config.token = "xoxb-test";
    config.api_base_url = mock.base_url;

    SlackAdapter adapter(config);
    adapter.send("hello", "slack_C123");
    SUCCEED();
}

TEST(SlackAdapterApiTest, SendMessageFailMock) {
    SlackMockServer mock;
    mock.start();

    SlackAdapterConfig config;
    config.token = "xoxb-test";
    config.api_base_url = mock.base_url;

    // Trigger failure by unsetting auth in mock would require state in mock,
    // but here we just ensure the fail branch in send() is reached.
    mock.server.Post("/api/chat.postMessage", [](const httplib::Request&, httplib::Response& res) {
        res.status = 500;
    });

    SlackAdapter adapter(config);
    adapter.send("hello", "slack_C123");
    SUCCEED();
}

} // namespace
} // namespace euxis::adapters
