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

} // namespace
} // namespace euxis::adapters
