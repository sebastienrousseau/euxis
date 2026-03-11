#include <gtest/gtest.h>
#include "euxis/adapters/telegram.hpp"
#include <httplib.h>
#include <thread>
#include <chrono>
#include <atomic>

namespace euxis::adapters {
namespace {

class TelegramMockServer {
public:
    TelegramMockServer() {
        server.Post("/bot-token/sendMessage", [](const httplib::Request& req, httplib::Response& res) {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("text") && body["text"] == "fail") {
                res.status = 400;
                res.set_content(R"({"ok":false,"description":"Error"})", "application/json");
            } else {
                res.set_content(R"({"ok":true,"result":{"message_id":123}})", "application/json");
            }
        });

        server.Post("/bot-token/setWebhook", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"ok":true,"result":true})", "application/json");
        });

        server.Post("/bot-token/deleteWebhook", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"ok":true,"result":true})", "application/json");
        });

        server.Post("/bot-token/getUpdates", [this](const httplib::Request&, httplib::Response& res) {
            if (poll_count.fetch_add(1) == 0) {
                res.set_content(R"({"ok":true,"result":[{"update_id":100,"message":{"chat":{"id":42,"type":"private"},"text":"hello"}}]})", "application/json");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                res.set_content(R"({"ok":true,"result":[]})", "application/json");
            }
        });
    }

    void start() {
        int port = server.bind_to_any_port("127.0.0.1");
        base_url = "http://127.0.0.1:" + std::to_string(port);
        thread = std::thread([this]() { server.listen_after_bind(); });
    }

    ~TelegramMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    httplib::Server server;
    std::string base_url;
    std::thread thread;
    std::atomic<int> poll_count{0};
};

TEST(TelegramAdapterApiTest, SendMessageMock) {
    TelegramMockServer mock;
    mock.start();

    TelegramAdapterConfig config;
    config.token = "-token";
    config.api_base_url = mock.base_url;

    TelegramAdapter adapter(config);
    adapter.send("hello", "telegram_42");
    SUCCEED();
}

TEST(TelegramAdapterApiTest, ConnectWebhookMock) {
    TelegramMockServer mock;
    mock.start();

    TelegramAdapterConfig config;
    config.token = "-token";
    config.mode = "webhook";
    config.webhook_url = "https://example.com/hook";
    config.api_base_url = mock.base_url;

    TelegramAdapter adapter(config);
    adapter.connect();
    SUCCEED();
}

TEST(TelegramAdapterApiTest, PollingLoopMock) {
    TelegramMockServer mock;
    mock.start();

    std::atomic<bool> received{false};
    TelegramAdapterConfig config;
    config.token = "-token";
    config.mode = "polling";
    config.poll_interval = 0.1;
    config.api_base_url = mock.base_url;

    TelegramAdapter adapter(config, [&](auto, auto text, auto) {
        if (text == "hello") received = true;
    });

    adapter.connect();
    
    // Wait for poll
    for (int i = 0; i < 20 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(received);
    adapter.disconnect();
}

} // namespace
} // namespace euxis::adapters
