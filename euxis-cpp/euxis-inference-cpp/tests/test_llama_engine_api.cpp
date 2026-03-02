#include <gtest/gtest.h>
#include "euxis/inference/llama_engine.hpp"
#include <httplib.h>
#include <thread>
#include <cstdlib>

namespace euxis::inference {
namespace {

class LlamaMockServer {
public:
    LlamaMockServer() {
        server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({"status":"ok"})", "application/json");
        });

        server.Post("/v1/chat/completions", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({
                "model": "test-model",
                "choices": [{"message": {"role": "assistant", "content": "hello world"}}],
                "usage": {"completion_tokens": 5}
            })", "application/json");
        });

        server.Get("/v1/models", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(R"({
                "data": [{"id": "test-model"}]
            })", "application/json");
        });
    }

    void start() {
        int port = server.bind_to_any_port("127.0.0.1");
        host = "127.0.0.1";
        port_str = std::to_string(port);
        thread = std::thread([this]() { server.listen_after_bind(); });
        
        setenv("LLAMA_SERVER_HOST", host.c_str(), 1);
        setenv("LLAMA_SERVER_PORT", port_str.c_str(), 1);
    }

    ~LlamaMockServer() {
        server.stop();
        if (thread.joinable()) thread.join();
        unsetenv("LLAMA_SERVER_HOST");
        unsetenv("LLAMA_SERVER_PORT");
    }

    httplib::Server server;
    std::string host;
    std::string port_str;
    std::thread thread;
};

TEST(LlamaEngineApiTest, GenerateMock) {
    LlamaMockServer mock;
    mock.start();

    LocalModelConfig cfg;
    cfg.model_name = "test-model";
    
    LlamaEngine engine(cfg);
    auto result = engine.generate("hi", 10);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->text, "hello world");
    EXPECT_EQ(result->tokens_generated, 5);
}

TEST(LlamaEngineApiTest, HealthMock) {
    LlamaMockServer mock;
    mock.start();

    LocalModelConfig cfg;
    LlamaEngine engine(cfg);
    auto h = engine.health();
    
    EXPECT_EQ(h["status"], "ok");
}

TEST(LlamaEngineApiTest, SupportsModelMock) {
    LlamaMockServer mock;
    mock.start();

    LocalModelConfig cfg;
    LlamaEngine engine(cfg);
    
    EXPECT_TRUE(engine.supports_model("test-model"));
    EXPECT_FALSE(engine.supports_model("other-model"));
}

TEST(LlamaEngineApiTest, ServerDown) {
    // No server started
    setenv("LLAMA_SERVER_HOST", "127.0.0.1", 1);
    setenv("LLAMA_SERVER_PORT", "1", 1); // Invalid port/no listener

    LocalModelConfig cfg;
    LlamaEngine engine(cfg);
    
    auto result = engine.generate("hi", 10);
    EXPECT_FALSE(result.has_value());
    
    auto h = engine.health();
    EXPECT_EQ(h["status"], "unreachable");
    
    unsetenv("LLAMA_SERVER_HOST");
    unsetenv("LLAMA_SERVER_PORT");
}

} // namespace
} // namespace euxis::inference
