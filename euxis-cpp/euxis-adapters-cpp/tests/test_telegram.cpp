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

} // namespace
} // namespace euxis::adapters
