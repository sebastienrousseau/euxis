#include <gtest/gtest.h>
#include "euxis/adapters/whatsapp.hpp"

namespace euxis::adapters {
namespace {

WhatsAppAdapterConfig make_test_config(bool enabled = true) {
    return WhatsAppAdapterConfig{
        .token = "",
        .phone_number_id = "",
        .verify_token = "",
        .enabled = enabled,
        .api_base_url = ""
    };
}

TEST(WhatsAppAdapterTest, ReceiveExtractsText) {
    std::string received;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto text, auto) { received = text; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "1234"}, {"id", "m1"}, {"text", {{"body", "hello"}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_EQ(received, "hello");
}

TEST(WhatsAppAdapterTest, EmptyTextIgnored) {
    bool called = false;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { called = true; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "1234"}, {"text", {{"body", ""}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_FALSE(called);
}

TEST(WhatsAppAdapterTest, MissingFromIgnored) {
    bool called = false;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { called = true; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"text", {{"body", "hello"}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_FALSE(called);
}

TEST(WhatsAppAdapterTest, SessionIdFormat) {
    std::string received_sid;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto sid, auto, auto) { received_sid = sid; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "5551234"}, {"text", {{"body", "hi"}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_EQ(received_sid, "whatsapp_5551234");
}

TEST(WhatsAppAdapterTest, MetaContainsSender) {
    nlohmann::json received_meta;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto meta) { received_meta = meta; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "5551234"}, {"text", {{"body", "hi"}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_EQ(received_meta["sender"], "5551234");
    EXPECT_EQ(received_meta["scope"], "dm");
}

TEST(WhatsAppAdapterTest, MultipleMessages) {
    int call_count = 0;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { ++call_count; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {
            {{"from", "111"}, {"text", {{"body", "msg1"}}}},
            {{"from", "222"}, {"text", {{"body", "msg2"}}}}
        }}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_EQ(call_count, 2);
}

TEST(WhatsAppAdapterTest, MultipleEntries) {
    int call_count = 0;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { ++call_count; });

    nlohmann::json msg1 = {{"from", "111"}, {"text", {{"body", "msg1"}}}};
    nlohmann::json msg2 = {{"from", "222"}, {"text", {{"body", "msg2"}}}};
    nlohmann::json entry1 = {{"changes", {{{"value", {{"messages", {msg1}}}}}}}};
    nlohmann::json entry2 = {{"changes", {{{"value", {{"messages", {msg2}}}}}}}};
    nlohmann::json payload = {{"entry", {entry1, entry2}}};
    adapter.receive(payload);
    EXPECT_EQ(call_count, 2);
}

TEST(WhatsAppAdapterTest, EmptyPayload) {
    bool called = false;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { called = true; });

    nlohmann::json payload = nlohmann::json::object();
    adapter.receive(payload);
    EXPECT_FALSE(called);
}

TEST(WhatsAppAdapterTest, MessageWithoutText) {
    bool called = false;
    WhatsAppAdapter adapter(
        make_test_config(),
        [&](auto, auto, auto) { called = true; });

    // Message type "image" without text
    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "5551234"}, {"type", "image"}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_FALSE(called); // No text body -> empty text -> skipped
}

TEST(WhatsAppAdapterTest, ConnectDisabled) {
    WhatsAppAdapterConfig config;
    config.enabled = false;
    config.token = "test-token";

    WhatsAppAdapter adapter(config);
    adapter.connect(); // Should log info and return
    SUCCEED();
}

TEST(WhatsAppAdapterTest, ConnectMissingToken) {
    WhatsAppAdapterConfig config;
    config.enabled = true;
    config.token = "";

    WhatsAppAdapter adapter(config);
    adapter.connect(); // Should log info and return
    SUCCEED();
}

TEST(WhatsAppAdapterTest, ConnectEnabled) {
    WhatsAppAdapterConfig config;
    config.enabled = true;
    config.token = "test-token";
    config.phone_number_id = "12345";

    WhatsAppAdapter adapter(config);
    adapter.connect();
    SUCCEED();
}

TEST(WhatsAppAdapterTest, SendMissingToken) {
    WhatsAppAdapterConfig config;
    config.token = "";
    config.phone_number_id = "12345";

    WhatsAppAdapter adapter(config);
    adapter.send("hello", "whatsapp_1234");
    SUCCEED();
}

TEST(WhatsAppAdapterTest, SendMissingPhoneNumberId) {
    WhatsAppAdapterConfig config;
    config.token = "test-token";
    config.phone_number_id = "";

    WhatsAppAdapter adapter(config);
    adapter.send("hello", "whatsapp_1234");
    SUCCEED();
}

TEST(WhatsAppAdapterTest, SendWithSessionMeta) {
    WhatsAppAdapterConfig config;
    config.token = "test-token";
    config.phone_number_id = "12345";

    WhatsAppAdapter adapter(config);

    // Receive first to establish session meta
    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "5551234"}, {"text", {{"body", "hi"}}}
        }}}}}}}}}}}};
    adapter.receive(payload);

    adapter.send("reply", "whatsapp_5551234");
    SUCCEED();
}

TEST(WhatsAppAdapterTest, SendResolvesFromSessionId) {
    WhatsAppAdapterConfig config;
    config.token = "test-token";
    config.phone_number_id = "12345";

    WhatsAppAdapter adapter(config);
    // No prior receive; should parse from session_id
    adapter.send("hello", "whatsapp_5559876");
    SUCCEED();
}

TEST(WhatsAppAdapterTest, SendInvalidSessionId) {
    WhatsAppAdapterConfig config;
    config.token = "test-token";
    config.phone_number_id = "12345";

    WhatsAppAdapter adapter(config);
    // Session ID without "whatsapp_" prefix
    adapter.send("hello", "unknown_session");
    SUCCEED(); // Should return early
}

TEST(WhatsAppAdapterTest, AckIsNoop) {
    WhatsAppAdapter adapter(make_test_config());
    adapter.ack("msg123");
    SUCCEED();
}

TEST(WhatsAppAdapterTest, DisconnectIsNoop) {
    WhatsAppAdapter adapter(make_test_config());
    adapter.disconnect();
    SUCCEED();
}

} // namespace
} // namespace euxis::adapters
