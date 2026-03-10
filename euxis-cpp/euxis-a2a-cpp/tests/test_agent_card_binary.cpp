#include <gtest/gtest.h>
#include "euxis/a2a/agent_card.hpp"

namespace euxis::a2a {
namespace {

TEST(AgentCardBinaryTest, MsgpackRoundtrip) {
    AgentCard card;
    card.name = "test-agent";
    card.description = "A test agent for binary serialization.";
    card.url = "http://localhost:8080";
    card.version = "1.0.0";
    card.capabilities.push_back({"cap1", "desc1", std::nullopt, std::nullopt});
    card.capabilities.push_back({"cap2", "desc2", std::nullopt, std::nullopt});

    auto data = card.to_msgpack();
    ASSERT_FALSE(data.empty());

    auto restored = AgentCard::from_msgpack(data);
    EXPECT_EQ(restored.name, card.name);
    EXPECT_EQ(restored.description, card.description);
    EXPECT_EQ(restored.url, card.url);
    EXPECT_EQ(restored.version, card.version);
    ASSERT_EQ(restored.capabilities.size(), 2u);
    EXPECT_EQ(restored.capabilities[0].name, "cap1");
    EXPECT_EQ(restored.capabilities[1].name, "cap2");
}

TEST(AgentCardBinaryTest, MsgpackEmptyCaps) {
    AgentCard card;
    card.name = "empty-caps";
    card.url = "ws://test";
    
    auto data = card.to_msgpack();
    auto restored = AgentCard::from_msgpack(data);
    EXPECT_TRUE(restored.capabilities.empty());
}

} // namespace
} // namespace euxis::a2a
