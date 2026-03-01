#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/identity/erc8004.hpp"

namespace euxis::identity {
namespace {

class ERC8004Test : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }
};

// --- generate_agent_card ---

TEST_F(ERC8004Test, GenerateCardBasic) {
    const auto card = generate_agent_card(
        "analyst-01",
        "Analyst Agent",
        "Performs data analysis tasks",
        {"data-analysis", "reporting"}
    );

    EXPECT_EQ(card.agent_id, "analyst-01");
    EXPECT_EQ(card.did, "did:euxis:analyst-01");
    EXPECT_EQ(card.name, "Analyst Agent");
    EXPECT_EQ(card.description, "Performs data analysis tasks");
    ASSERT_EQ(card.capabilities.size(), 2);
    EXPECT_EQ(card.capabilities[0], "data-analysis");
    EXPECT_EQ(card.capabilities[1], "reporting");
    EXPECT_EQ(card.version, "1.0.0");
    EXPECT_TRUE(card.metadata.is_object());
    EXPECT_TRUE(card.metadata.empty());
}

TEST_F(ERC8004Test, GenerateCardCustomVersion) {
    const auto card = generate_agent_card(
        "v2-agent",
        "V2 Agent",
        "Second generation",
        {"inference"},
        "2.1.0"
    );

    EXPECT_EQ(card.version, "2.1.0");
}

TEST_F(ERC8004Test, GenerateCardEmptyCapabilities) {
    const auto card = generate_agent_card(
        "bare-agent", "Bare Agent", "No capabilities", {});

    EXPECT_TRUE(card.capabilities.empty());
}

TEST_F(ERC8004Test, GenerateCardDIDFormat) {
    const auto card = generate_agent_card(
        "my-agent", "My Agent", "Test", {"test"});

    EXPECT_TRUE(card.did.starts_with("did:euxis:"));
    EXPECT_EQ(card.did, "did:euxis:my-agent");
}

TEST_F(ERC8004Test, GenerateCardManyCapabilities) {
    std::vector<std::string> caps = {
        "inference", "tool-use", "code-generation", "data-analysis",
        "web-search", "file-io", "a2a-communication", "memory"
    };

    const auto card = generate_agent_card(
        "super-agent", "Super Agent", "Can do everything", caps);

    ASSERT_EQ(card.capabilities.size(), 8);
    EXPECT_EQ(card.capabilities[0], "inference");
    EXPECT_EQ(card.capabilities[7], "memory");
}

// --- JSON format ---

TEST_F(ERC8004Test, ToJsonContainsAllFields) {
    const auto card = generate_agent_card(
        "json-agent",
        "JSON Agent",
        "Test JSON output",
        {"cap-a", "cap-b"},
        "1.2.3"
    );
    const auto j = card.to_json();

    EXPECT_EQ(j["agentId"], "json-agent");
    EXPECT_EQ(j["did"], "did:euxis:json-agent");
    EXPECT_EQ(j["name"], "JSON Agent");
    EXPECT_EQ(j["description"], "Test JSON output");
    EXPECT_EQ(j["version"], "1.2.3");
    ASSERT_EQ(j["capabilities"].size(), 2);
    EXPECT_EQ(j["capabilities"][0], "cap-a");
    EXPECT_EQ(j["capabilities"][1], "cap-b");
    EXPECT_TRUE(j.contains("metadata"));
    EXPECT_TRUE(j["metadata"].is_object());
}

TEST_F(ERC8004Test, ToJsonSerializesAndParses) {
    const auto card = generate_agent_card(
        "roundtrip-agent", "Roundtrip", "Test roundtrip", {"cap"});
    const auto j = card.to_json();
    const auto serialized = j.dump();
    const auto parsed = nlohmann::json::parse(serialized);

    EXPECT_EQ(parsed, j);
    EXPECT_EQ(parsed["agentId"], "roundtrip-agent");
}

TEST_F(ERC8004Test, ToJsonEmptyCapabilitiesIsArray) {
    const auto card = generate_agent_card(
        "empty-cap", "Empty", "No caps", {});
    const auto j = card.to_json();

    EXPECT_TRUE(j["capabilities"].is_array());
    EXPECT_TRUE(j["capabilities"].empty());
}

TEST_F(ERC8004Test, ToJsonMetadataIsExtensible) {
    auto card = generate_agent_card(
        "meta-agent", "Meta Agent", "With metadata", {"test"});

    // Metadata should be extensible
    card.metadata["custom_field"] = "custom_value";
    card.metadata["score"] = 42;

    const auto j = card.to_json();
    EXPECT_EQ(j["metadata"]["custom_field"], "custom_value");
    EXPECT_EQ(j["metadata"]["score"], 42);
}

TEST_F(ERC8004Test, MultipleCardsHaveSameFormatDifferentData) {
    const auto card1 = generate_agent_card(
        "agent-a", "Agent A", "First", {"x"});
    const auto card2 = generate_agent_card(
        "agent-b", "Agent B", "Second", {"y"});

    const auto j1 = card1.to_json();
    const auto j2 = card2.to_json();

    // Same keys
    for (const auto& key : {"agentId", "did", "name", "description",
                             "capabilities", "version", "metadata"}) {
        EXPECT_TRUE(j1.contains(key)) << "Missing key: " << key;
        EXPECT_TRUE(j2.contains(key)) << "Missing key: " << key;
    }

    // Different values
    EXPECT_NE(j1["agentId"], j2["agentId"]);
    EXPECT_NE(j1["did"], j2["did"]);
}

} // namespace
} // namespace euxis::identity
