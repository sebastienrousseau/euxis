#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "euxis/gateway/config.hpp"

namespace euxis::gateway {
namespace {

TEST(GatewayConfigTest, DefaultValues) {
    GatewayConfig c;
    EXPECT_EQ(c.port, 8080);
    EXPECT_EQ(c.host, "0.0.0.0");
    EXPECT_EQ(c.timeouts.webhook, 5);
}

TEST(GatewayConfigTest, FromJson) {
    nlohmann::json j = {
        {"port", 9090},
        {"host", "127.0.0.1"},
        {"timeouts", {{"webhook", 10}}},
    };
    auto c = GatewayConfig::from_json(j);
    EXPECT_EQ(c.port, 9090);
    EXPECT_EQ(c.host, "127.0.0.1");
    EXPECT_EQ(c.timeouts.webhook, 10);
}

// --- Coverage: config.cpp lines 23-29 (load_from_file with valid JSON file) ---
TEST(GatewayConfigTest, LoadFromFile) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_config_test.json";
    {
        std::ofstream f(tmp);
        f << R"({"port": 7070, "host": "10.0.0.1", "ws_port": 7071, "timeouts": {"webhook": 15, "api_call": 20}})";
    }
    auto c = GatewayConfig::load_from_file(tmp.string());
    EXPECT_EQ(c.port, 7070);
    EXPECT_EQ(c.host, "10.0.0.1");
    EXPECT_EQ(c.ws_port, 7071);
    EXPECT_EQ(c.timeouts.webhook, 15);
    EXPECT_EQ(c.timeouts.api_call, 20);
    std::filesystem::remove(tmp);
}

// --- Coverage: config.cpp line 25 (load_from_file with nonexistent file) ---
TEST(GatewayConfigTest, LoadFromFileNonexistent) {
    auto c = GatewayConfig::load_from_file("/tmp/euxis_no_such_config_file.json");
    // Returns default config when file doesn't exist
    EXPECT_EQ(c.port, 8080);
    EXPECT_EQ(c.host, "0.0.0.0");
}

// --- Coverage: config.cpp line 21 (from_json without timeouts section) ---
TEST(GatewayConfigTest, FromJsonWithoutTimeouts) {
    nlohmann::json j = {{"port", 5000}, {"host", "localhost"}};
    auto c = GatewayConfig::from_json(j);
    EXPECT_EQ(c.port, 5000);
    EXPECT_EQ(c.host, "localhost");
    // Timeouts should be defaults
    EXPECT_EQ(c.timeouts.webhook, 5);
    EXPECT_EQ(c.timeouts.health_check, 2);
    EXPECT_EQ(c.timeouts.api_call, 10);
    EXPECT_EQ(c.timeouts.tts_command, 30);
}

// --- Coverage: config.cpp lines 15-18 (from_json with all timeout fields) ---
TEST(GatewayConfigTest, FromJsonAllTimeouts) {
    nlohmann::json j = {
        {"port", 9999},
        {"timeouts", {
            {"webhook", 1},
            {"health_check", 3},
            {"api_call", 5},
            {"tts_command", 7}
        }},
    };
    auto c = GatewayConfig::from_json(j);
    EXPECT_EQ(c.timeouts.webhook, 1);
    EXPECT_EQ(c.timeouts.health_check, 3);
    EXPECT_EQ(c.timeouts.api_call, 5);
    EXPECT_EQ(c.timeouts.tts_command, 7);
}

} // namespace
} // namespace euxis::gateway
