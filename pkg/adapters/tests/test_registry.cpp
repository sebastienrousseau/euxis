#include <gtest/gtest.h>
#include "euxis/adapters/registry.hpp"

namespace euxis::adapters {
namespace {

TEST(RegistryTest, EmptyConfigReturnsNoAdapters) {
    auto adapters = build_adapters({});
    EXPECT_TRUE(adapters.empty());
}

TEST(RegistryTest, EnabledSlackCreatesAdapter) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"slack", {
            {"enabled", true}, {"token", "t"}, {"app_token", "a"}
        }}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("slack"), 1);
}

TEST(RegistryTest, DisabledAdapterNotCreated) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"slack", {{"enabled", false}}}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("slack"), 0);
}

// --- Coverage: registry.cpp lines 27-38 (telegram adapter creation) ---
TEST(RegistryTest, EnabledTelegramCreatesAdapter) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"telegram", {
            {"enabled", true}, {"token", "tg-token"}, {"mode", "webhook"},
            {"webhook_url", "https://example.com/webhook"},
            {"poll_timeout", 30}, {"poll_interval", 2.0}
        }}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("telegram"), 1);
}

// --- Coverage: registry.cpp lines 40-47 (discord adapter creation) ---
TEST(RegistryTest, EnabledDiscordCreatesAdapter) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"discord", {
            {"enabled", true}, {"token", "dc-token"}
        }}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("discord"), 1);
}

// --- Coverage: registry.cpp lines 49-59 (whatsapp adapter creation) ---
TEST(RegistryTest, EnabledWhatsappCreatesAdapter) {
    nlohmann::json config = {
        {"gateway", {{"channels", {{"whatsapp", {
            {"enabled", true}, {"token", "wa-token"},
            {"phone_number_id", "12345"}, {"verify_token", "verify-me"}
        }}}}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("whatsapp"), 1);
}

// --- Coverage: registry.cpp (all four channels enabled simultaneously) ---
TEST(RegistryTest, AllChannelsEnabled) {
    nlohmann::json config = {
        {"gateway", {{"channels", {
            {"slack", {{"enabled", true}, {"token", "s"}, {"app_token", "a"}}},
            {"telegram", {{"enabled", true}, {"token", "t"}}},
            {"discord", {{"enabled", true}, {"token", "d"}}},
            {"whatsapp", {{"enabled", true}, {"token", "w"}, {"phone_number_id", "p"}, {"verify_token", "v"}}},
        }}}},
    };
    auto adapters = build_adapters(config);
    EXPECT_EQ(adapters.count("slack"), 1);
    EXPECT_EQ(adapters.count("telegram"), 1);
    EXPECT_EQ(adapters.count("discord"), 1);
    EXPECT_EQ(adapters.count("whatsapp"), 1);
    EXPECT_EQ(adapters.size(), 4u);
}

// --- Coverage: registry.cpp line 7-8 (non-object config returns empty) ---
TEST(RegistryTest, NonObjectConfigReturnsEmpty) {
    auto adapters = build_adapters(nlohmann::json::array());
    EXPECT_TRUE(adapters.empty());
}

// --- Coverage: registry.cpp (config with no gateway key) ---
TEST(RegistryTest, MissingGatewayKeyReturnsEmpty) {
    nlohmann::json config = {{"other", "data"}};
    auto adapters = build_adapters(config);
    EXPECT_TRUE(adapters.empty());
}

} // namespace
} // namespace euxis::adapters
