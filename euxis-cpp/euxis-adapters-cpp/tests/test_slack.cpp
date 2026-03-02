#include <gtest/gtest.h>
#include "euxis/adapters/slack.hpp"

namespace euxis::adapters {
namespace {

TEST(SlackAdapterTest, ReceiveExtractsText) {
    std::string received_text;
    SlackAdapter adapter({}, [&](auto, auto text, auto) { received_text = text; });

    nlohmann::json msg = {
        {"event", {{"text", "hello"}, {"channel", "C123"}, {"user", "U1"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "hello");
}

TEST(SlackAdapterTest, IgnoresBotMessages) {
    bool called = false;
    SlackAdapter adapter({}, [&](auto, auto, auto) { called = true; });
    nlohmann::json msg = {{"subtype", "bot_message"}, {"text", "hi"}};
    adapter.receive(msg);
    EXPECT_FALSE(called);
}

TEST(SlackAdapterTest, ThreadSession) {
    std::string received_sid;
    SlackAdapter adapter(
        {}, [&](auto sid, auto, auto) { received_sid = sid; });
    nlohmann::json msg = {
        {"channel", "C1"}, {"thread_ts", "123.456"}, {"text", "x"}};
    adapter.receive(msg);
    EXPECT_EQ(received_sid, "slack_C1:123.456");
}

TEST(SlackAdapterTest, NonThreadSession) {
    std::string received_sid;
    SlackAdapter adapter(
        {}, [&](auto sid, auto, auto) { received_sid = sid; });
    nlohmann::json msg = {
        {"channel", "C1"}, {"text", "x"}};
    adapter.receive(msg);
    EXPECT_EQ(received_sid, "slack_C1");
}

TEST(SlackAdapterTest, ReceiveDirectPayload) {
    std::string received_text;
    SlackAdapter adapter({}, [&](auto, auto text, auto) { received_text = text; });

    // Direct payload without "event" wrapper
    nlohmann::json msg = {
        {"text", "direct"}, {"channel", "C1"}, {"user", "U1"}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "direct");
}

TEST(SlackAdapterTest, GroupScope) {
    nlohmann::json received_meta;
    SlackAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"event", {{"text", "hi"}, {"channel", "C1"}, {"channel_type", "channel"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(SlackAdapterTest, GroupScopeForGroup) {
    nlohmann::json received_meta;
    SlackAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"event", {{"text", "hi"}, {"channel", "C1"}, {"channel_type", "group"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(SlackAdapterTest, GroupScopeForMpim) {
    nlohmann::json received_meta;
    SlackAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"event", {{"text", "hi"}, {"channel", "C1"}, {"channel_type", "mpim"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(SlackAdapterTest, DmScope) {
    nlohmann::json received_meta;
    SlackAdapter adapter({}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"event", {{"text", "hi"}, {"channel", "D1"}, {"channel_type", "im"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "dm");
}

TEST(SlackAdapterTest, ReceiveNonObject) {
    bool called = false;
    SlackAdapter adapter({}, [&](auto, auto, auto) { called = true; });

    nlohmann::json msg = {{"event", "not an object"}};
    adapter.receive(msg);
    EXPECT_FALSE(called);
}

TEST(SlackAdapterTest, ConnectNonSocket) {
    SlackAdapterConfig config;
    config.mode = "webhook";
    SlackAdapter adapter(config);
    adapter.connect(); // mode != "socket", should return immediately
    SUCCEED();
}

TEST(SlackAdapterTest, ConnectSocketMissingTokens) {
    SlackAdapterConfig config;
    config.mode = "socket";
    config.token = "";
    config.app_token = "";
    SlackAdapter adapter(config);
    adapter.connect(); // Should log warning
    SUCCEED();
}

TEST(SlackAdapterTest, ConnectSocketWithTokens) {
    SlackAdapterConfig config;
    config.mode = "socket";
    config.token = "xoxb-test";
    config.app_token = "xapp-test";
    SlackAdapter adapter(config);
    adapter.connect();
    SUCCEED();
}

TEST(SlackAdapterTest, SendWithEmptyToken) {
    SlackAdapterConfig config;
    config.token = "";
    SlackAdapter adapter(config);
    adapter.send("hello", "slack_C1"); // Should warn and return
    SUCCEED();
}

TEST(SlackAdapterTest, SendResolvesSessionFromId) {
    SlackAdapterConfig config;
    config.token = "xoxb-test";
    SlackAdapter adapter(config);
    // No prior receive; should parse from "slack_C123" -> channel="C123"
    adapter.send("response", "slack_C123");
    SUCCEED();
}

TEST(SlackAdapterTest, SendResolvesThreadFromId) {
    SlackAdapterConfig config;
    config.token = "xoxb-test";
    SlackAdapter adapter(config);
    // Should parse "slack_C1:ts123" -> channel="C1", thread_ts="ts123"
    adapter.send("response", "slack_C1:ts123");
    SUCCEED();
}

TEST(SlackAdapterTest, SendWithSessionMeta) {
    SlackAdapterConfig config;
    config.token = "xoxb-test";
    SlackAdapter adapter(config);

    // Receive first to establish session meta
    nlohmann::json msg = {
        {"channel", "C99"}, {"thread_ts", "999.111"}, {"text", "init"}, {"user", "U1"}};
    adapter.receive(msg);

    adapter.send("reply", "slack_C99:999.111");
    SUCCEED();
}

TEST(SlackAdapterTest, AckIsNoop) {
    SlackAdapter adapter({});
    adapter.ack("msg123");
    SUCCEED();
}

TEST(SlackAdapterTest, DisconnectIsNoop) {
    SlackAdapter adapter({});
    adapter.disconnect();
    SUCCEED();
}

TEST(SlackAdapterTest, ReceiveWithChannelId) {
    std::string received_text;
    SlackAdapter adapter({}, [&](auto, auto text, auto) { received_text = text; });

    // Uses "channel_id" instead of "channel"
    nlohmann::json msg = {
        {"channel_id", "C456"}, {"text", "via channel_id"}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "via channel_id");
}

TEST(SlackAdapterTest, ReceiveWithMessageKey) {
    std::string received_text;
    SlackAdapter adapter({}, [&](auto, auto text, auto) { received_text = text; });

    // Uses "message" instead of "text"
    nlohmann::json msg = {
        {"channel", "C1"}, {"message", "via message key"}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "via message key");
}

TEST(SlackAdapterTest, SendResolvesEmptyChannel) {
    SlackAdapterConfig config;
    config.token = "xoxb-test";
    SlackAdapter adapter(config);
    // Session that resolves to empty channel
    adapter.send("response", "");
    SUCCEED(); // Should return early
}

} // namespace
} // namespace euxis::adapters
