#include <gtest/gtest.h>
#include "euxis/adapters/telegram.hpp"

namespace euxis::adapters {
namespace {

TEST(TelegramAdapterTest, ReceiveExtractsText) {
    std::string received_text;
    TelegramAdapter adapter(
        {}, [&](auto, auto text, auto) { received_text = text; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 42}, {"type", "private"}}},
                     {"text", "hello"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "hello");
}

TEST(TelegramAdapterTest, GroupScope) {
    nlohmann::json received_meta;
    TelegramAdapter adapter(
        {}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 1}, {"type", "supergroup"}}},
                     {"text", "hi"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(TelegramAdapterTest, GroupScopeRegular) {
    nlohmann::json received_meta;
    TelegramAdapter adapter(
        {}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 1}, {"type", "group"}}},
                     {"text", "hi"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "group");
}

TEST(TelegramAdapterTest, DmScope) {
    nlohmann::json received_meta;
    TelegramAdapter adapter(
        {}, [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 42}, {"type", "private"}}},
                     {"text", "dm"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_meta["scope"], "dm");
}

TEST(TelegramAdapterTest, SessionIdFormat) {
    std::string received_sid;
    TelegramAdapter adapter(
        {}, [&](auto sid, auto, auto) { received_sid = sid; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 12345}, {"type", "private"}}},
                     {"text", "test"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_sid, "telegram_12345");
}

TEST(TelegramAdapterTest, ReceiveWithCaption) {
    std::string received_text;
    TelegramAdapter adapter(
        {}, [&](auto, auto text, auto) { received_text = text; });

    // Message with caption but no text
    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 42}, {"type", "private"}}},
                     {"caption", "photo caption"}}}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "photo caption");
}

TEST(TelegramAdapterTest, ReceiveDirectMessage) {
    // Message without "message" wrapper (direct payload)
    std::string received_text;
    TelegramAdapter adapter(
        {}, [&](auto, auto text, auto) { received_text = text; });

    nlohmann::json msg = {
        {"chat", {{"id", 42}, {"type", "private"}}},
        {"text", "direct"}};
    adapter.receive(msg);
    EXPECT_EQ(received_text, "direct");
}

TEST(TelegramAdapterTest, ReceiveNoChatId) {
    bool called = false;
    TelegramAdapter adapter(
        {}, [&](auto, auto, auto) { called = true; });

    nlohmann::json msg = {
        {"message", {{"chat", {{"type", "private"}}},
                     {"text", "no id"}}}};
    adapter.receive(msg);
    EXPECT_FALSE(called); // chat_id == 0, should be skipped
}

TEST(TelegramAdapterTest, ReceiveNonObject) {
    bool called = false;
    TelegramAdapter adapter(
        {}, [&](auto, auto, auto) { called = true; });

    nlohmann::json msg = "just a string";
    adapter.receive(msg);
    EXPECT_FALSE(called);
}

TEST(TelegramAdapterTest, ConnectWithEmptyToken) {
    TelegramAdapter adapter({});
    adapter.connect(); // Should log warning and return
    SUCCEED();
}

TEST(TelegramAdapterTest, ConnectWebhookNoUrl) {
    TelegramAdapterConfig config;
    config.mode = "webhook";
    config.token = "test-token";
    // webhook_url is empty

    TelegramAdapter adapter(config);
    adapter.connect(); // Should log warning and return
    SUCCEED();
}

TEST(TelegramAdapterTest, SendWithEmptyToken) {
    TelegramAdapter adapter({});
    adapter.send("hello", "telegram_42"); // Should return silently
    SUCCEED();
}

TEST(TelegramAdapterTest, AckIsNoop) {
    TelegramAdapter adapter({});
    adapter.ack("msg123"); // Should be a no-op
    SUCCEED();
}

TEST(TelegramAdapterTest, DisconnectSafe) {
    TelegramAdapter adapter({});
    adapter.disconnect();
    SUCCEED();
}

TEST(TelegramAdapterTest, DisconnectWebhookMode) {
    TelegramAdapterConfig config;
    config.mode = "webhook";
    config.token = "test-token";

    TelegramAdapter adapter(config);
    adapter.disconnect(); // Should try to delete webhook
    SUCCEED();
}

TEST(TelegramAdapterTest, SendWithSessionMeta) {
    TelegramAdapter adapter({});

    // First receive to establish session metadata
    nlohmann::json msg = {
        {"message", {{"chat", {{"id", 42}, {"type", "private"}}},
                     {"text", "hello"}}}};
    adapter.receive(msg);

    // Now send (will fail since no real token, but should not crash)
    adapter.send("response", "telegram_42");
    SUCCEED();
}

TEST(TelegramAdapterTest, SendResolvesFromSessionId) {
    TelegramAdapterConfig config;
    config.token = "test-token";
    TelegramAdapter adapter(config);

    // Send without prior receive; should parse chat_id from session_id
    // Will fail to actually send (no real API), but should not crash
    adapter.send("hello", "telegram_99");
    SUCCEED();
}

TEST(TelegramAdapterTest, SendInvalidSessionId) {
    TelegramAdapterConfig config;
    config.token = "test-token";
    TelegramAdapter adapter(config);

    // Session ID that can't be parsed to a chat_id
    adapter.send("hello", "invalid_session");
    SUCCEED();
}

} // namespace
} // namespace euxis::adapters
