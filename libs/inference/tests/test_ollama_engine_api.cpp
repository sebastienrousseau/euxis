#include <gtest/gtest.h>
#include "euxis/inference/ollama_engine.hpp"
#include <httplib.h>
#include <thread>

namespace euxis::inference {
namespace {

class OllamaMockServer {
public:
    OllamaMockServer() {}

    void start() {
        port = server.bind_to_any_port("127.0.0.1");
        host = "127.0.0.1";
        thread = std::thread([this]() { server.listen_after_bind(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~OllamaMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    httplib::Server server;
    std::string host;
    int port{0};
    std::thread thread;
};

TEST(OllamaEngineApiTest, GenerateMock) {
    OllamaMockServer mock;
    mock.server.Post("/api/generate", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({
            "model": "llama3",
            "response": "ollama response",
            "eval_count": 10,
            "eval_duration": 1000000000
        })", "application/json");
    });
    mock.start();

    OllamaEngine engine(mock.host, mock.port);
    auto result = engine.generate("hi", 10);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->text, "ollama response");
    EXPECT_EQ(result->tokens_generated, 10);
    EXPECT_NEAR(result->tokens_per_second, 10.0f, 0.1f);
}

TEST(OllamaEngineApiTest, HealthMock) {
    OllamaMockServer mock;
    mock.server.Get("/api/tags", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({
            "models": [{"name": "llama3:latest"}, {"name": "mistral"}]
        })", "application/json");
    });
    mock.start();

    OllamaEngine engine(mock.host, mock.port);
    auto h = engine.health();
    
    EXPECT_EQ(h["status"], "ok");
    EXPECT_GE(h["models"].size(), 1);
}

TEST(OllamaEngineApiTest, SupportsModelMock) {
    OllamaMockServer mock;
    mock.server.Get("/api/tags", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({
            "models": [{"name": "llama3:latest"}, {"name": "mistral"}]
        })", "application/json");
    });
    mock.start();

    OllamaEngine engine(mock.host, mock.port);
    
    EXPECT_TRUE(engine.supports_model("llama3"));
    EXPECT_TRUE(engine.supports_model("llama3:latest"));
    EXPECT_TRUE(engine.supports_model("mistral"));
    EXPECT_FALSE(engine.supports_model("nonexistent"));
}

TEST(OllamaEngineApiTest, ApiErrors) {
    OllamaMockServer mock;
    mock.server.Post("/api/generate", [](const httplib::Request&, httplib::Response& res) {
        res.status = 500;
        res.set_content("Internal Error", "text/plain");
    });
    mock.start();

    OllamaEngine engine(mock.host, mock.port);
    auto result = engine.generate("hi", 10);
    if (result.has_value()) {
        std::cerr << "Unexpected success! Text: " << result->text << std::endl;
    }
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        EXPECT_NE(result.error().find("500"), std::string::npos);
    }
}

TEST(OllamaEngineApiTest, ParseError) {
    OllamaMockServer mock;
    mock.server.Get("/api/tags", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("not json", "text/plain");
    });
    mock.start();

    OllamaEngine engine(mock.host, mock.port);
    auto h = engine.health();
    EXPECT_EQ(h["status"], "parse_error");
    
    EXPECT_FALSE(engine.supports_model("any"));
}

} // namespace
} // namespace euxis::inference
