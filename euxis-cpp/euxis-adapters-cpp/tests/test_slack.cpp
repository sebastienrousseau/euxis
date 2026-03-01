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

} // namespace
} // namespace euxis::adapters
