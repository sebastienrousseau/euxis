#include <gtest/gtest.h>
#include "euxis/adapters/discord.hpp"

namespace euxis::adapters {
namespace {

TEST(DiscordAdapterTest, ReceiveExtractsContent) {
    std::string received;
    DiscordAdapter adapter({}, [&](auto, auto text, auto) { received = text; });

    nlohmann::json msg = {
        {"channel_id", "123"}, {"author_id", "456"}, {"content", "hello"}};
    adapter.receive(msg);
    EXPECT_EQ(received, "hello");
}

TEST(DiscordAdapterTest, SessionId) {
    std::string sid;
    DiscordAdapter adapter({}, [&](auto s, auto, auto) { sid = s; });
    nlohmann::json msg = {{"channel_id", "C99"}, {"content", "x"}};
    adapter.receive(msg);
    EXPECT_EQ(sid, "discord_C99");
}

TEST(DiscordAdapterTest, PrivateDmScope) {
    nlohmann::json received_meta;
    DiscordAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"channel_id", "123"}, {"author_id", "456"}, {"content", "dm"},
        {"channel_type", "private"}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "dm");
}

TEST(DiscordAdapterTest, GroupScope) {
    nlohmann::json received_meta;
    DiscordAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"channel_id", "123"}, {"author_id", "456"}, {"content", "group msg"},
        {"channel_type", "guild"}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(DiscordAdapterTest, MetaContainsSender) {
    nlohmann::json received_meta;
    DiscordAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"channel_id", "123"}, {"author_id", "user789"}, {"content", "hi"}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["sender"], "user789");
}

TEST(DiscordAdapterTest, ConnectDisabled) {
    DiscordAdapterConfig config;
    config.enabled = false;
    config.token = "test-token";

    DiscordAdapter adapter(config);
    adapter.connect(); // Should log info and return
    SUCCEED();
}

TEST(DiscordAdapterTest, ConnectEmptyToken) {
    DiscordAdapterConfig config;
    config.enabled = true;
    config.token = "";

    DiscordAdapter adapter(config);
    adapter.connect(); // Should log info and return
    SUCCEED();
}

TEST(DiscordAdapterTest, ConnectEnabled) {
    DiscordAdapterConfig config;
    config.enabled = true;
    config.token = "test-token";

    DiscordAdapter adapter(config);
    adapter.connect();
    SUCCEED();
}

TEST(DiscordAdapterTest, SendWithEmptyToken) {
    DiscordAdapterConfig config;
    config.token = "";

    DiscordAdapter adapter(config);
    adapter.send("hello", "discord_123"); // Should log warning and return
    SUCCEED();
}

TEST(DiscordAdapterTest, SendExtractsChannelFromSessionId) {
    DiscordAdapterConfig config;
    config.token = "test-token";

    DiscordAdapter adapter(config);
    // No prior receive, so channel_id from session_id "discord_456"
    adapter.send("hello", "discord_456");
    SUCCEED(); // Should not crash; API call will fail but that's expected
}

TEST(DiscordAdapterTest, SendWithSessionMeta) {
    DiscordAdapterConfig config;
    config.token = "test-token";

    DiscordAdapter adapter(config);

    // Receive a message first to set session meta
    nlohmann::json msg = {
        {"channel_id", "789"}, {"author_id", "user1"}, {"content", "init"}};
    adapter.receive(msg);

    // Now send to the session
    adapter.send("response", "discord_789");
    SUCCEED();
}

TEST(DiscordAdapterTest, SendNoChannelId) {
    DiscordAdapterConfig config;
    config.token = "test-token";

    DiscordAdapter adapter(config);
    // Session ID that doesn't start with "discord_" and has no meta
    adapter.send("hello", "unknown_session");
    SUCCEED(); // Should log warning and return
}

TEST(DiscordAdapterTest, AckIsNoop) {
    DiscordAdapter adapter({});
    adapter.ack("msg123");
    SUCCEED();
}

TEST(DiscordAdapterTest, DisconnectIsNoop) {
    DiscordAdapter adapter({});
    adapter.disconnect();
    SUCCEED();
}

TEST(DiscordAdapterTest, NoHandlerNoCall) {
    DiscordAdapter adapter({});
    nlohmann::json msg = {
        {"channel_id", "123"}, {"content", "test"}};
    adapter.receive(msg);
    SUCCEED(); // Should not crash with null handler
}

} // namespace
} // namespace euxis::adapters
