#include <gtest/gtest.h>
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

} // namespace
} // namespace euxis::gateway
