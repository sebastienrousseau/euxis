#include <gtest/gtest.h>
#include "euxis/adapters/whatsapp.hpp"

namespace euxis::adapters {
namespace {

TEST(WhatsAppAdapterTest, ReceiveExtractsText) {
    std::string received;
    WhatsAppAdapter adapter(
        WhatsAppAdapterConfig{.token = "",
                              .phone_number_id = "",
                              .verify_token = "",
                              .enabled = true},
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
        WhatsAppAdapterConfig{.token = "",
                              .phone_number_id = "",
                              .verify_token = "",
                              .enabled = true},
        [&](auto, auto, auto) { called = true; });

    nlohmann::json payload = {
        {"entry", {{{"changes", {{{"value", {{"messages", {{
            {"from", "1234"}, {"text", {{"body", ""}}}
        }}}}}}}}}}}};
    adapter.receive(payload);
    EXPECT_FALSE(called);
}

} // namespace
} // namespace euxis::adapters
