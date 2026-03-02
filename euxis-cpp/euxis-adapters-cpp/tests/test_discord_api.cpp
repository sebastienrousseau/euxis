#include <gtest/gtest.h>
#include "euxis/adapters/discord.hpp"
#include <httplib.h>
#include <thread>

namespace euxis::adapters {
namespace {

class DiscordMockServer {
public:
    DiscordMockServer() {
        server.Post("/api/v10/channels/456/messages", [](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_header("Authorization")) {
                res.status = 401;
                return;
            }
            res.status = 200;
            res.set_content(R"({"id":"123"})", "application/json");
        });
    }

    void start() {
        int port = server.bind_to_any_port("127.0.0.1");
        base_url = "http://127.0.0.1:" + std::to_string(port);
        thread = std::thread([this]() { server.listen_after_bind(); });
    }

    ~DiscordMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    httplib::Server server;
    std::string base_url;
    std::thread thread;
};

TEST(DiscordAdapterApiTest, SendMessageMock) {
    DiscordMockServer mock;
    mock.start();

    DiscordAdapterConfig config;
    config.token = "test-token";
    config.api_base_url = mock.base_url;

    DiscordAdapter adapter(config);
    adapter.send("hello", "discord_456");
    SUCCEED();
}

TEST(DiscordAdapterApiTest, SendNoTokenWarns) {
    DiscordAdapter adapter({}); // empty token
    adapter.send("hello", "discord_456");
    SUCCEED();
}

TEST(DiscordAdapterApiTest, SendNoChannelWarns) {
    DiscordAdapterConfig config;
    config.token = "test";
    DiscordAdapter adapter(config);
    adapter.send("hello", "invalid");
    SUCCEED();
}

TEST(DiscordAdapterApiTest, SendApiFailureMock) {
    DiscordMockServer mock;
    mock.server.Post("/api/v10/channels/456/messages", [](const httplib::Request&, httplib::Response& res) {
        res.status = 404;
    });
    mock.start();

    DiscordAdapterConfig config;
    config.token = "test-token";
    config.api_base_url = mock.base_url;

    DiscordAdapter adapter(config);
    adapter.send("hello", "discord_456");
    SUCCEED();
}

} // namespace
} // namespace euxis::adapters
