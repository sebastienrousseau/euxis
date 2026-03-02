#include <gtest/gtest.h>

#include <string>

#include "euxis/a2a/agent_card.hpp"

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// Helper: build a minimal valid AgentCard.
// ---------------------------------------------------------------------------
auto make_valid_card() -> AgentCard {
    AgentCard card;
    card.name = "test-agent";
    card.description = "A test agent for unit tests";
    card.url = "http://localhost:8080";
    card.version = "0.2.0";

    Capability cap;
    cap.name = "echo";
    cap.description = "Echoes input back";
    cap.input_schema = nlohmann::json{{"type", "string"}};
    cap.output_schema = nlohmann::json{{"type", "string"}};
    card.capabilities.push_back(std::move(cap));

    AuthSchema auth;
    auth.type = "bearer";
    auth.description = "Bearer token authentication";
    card.auth = std::move(auth);

    card.identity_did = "did:euxis:test-agent-001";
    card.metadata = {{"env", "test"}};

    return card;
}

// ---------------------------------------------------------------------------
// to_json / from_json roundtrip
// ---------------------------------------------------------------------------
TEST(AgentCardTest, RoundtripSerialization) {
    const auto original = make_valid_card();
    const auto j = original.to_json();
    const auto restored = AgentCard::from_json(j);

    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.description, original.description);
    EXPECT_EQ(restored.url, original.url);
    EXPECT_EQ(restored.version, original.version);
    ASSERT_EQ(restored.capabilities.size(), original.capabilities.size());
    EXPECT_EQ(restored.capabilities[0].name, "echo");
    EXPECT_EQ(restored.capabilities[0].description, "Echoes input back");
    ASSERT_TRUE(restored.capabilities[0].input_schema.has_value());
    EXPECT_EQ(*restored.capabilities[0].input_schema,
              (nlohmann::json{{"type", "string"}}));
    ASSERT_TRUE(restored.capabilities[0].output_schema.has_value());
    EXPECT_EQ(*restored.capabilities[0].output_schema,
              (nlohmann::json{{"type", "string"}}));
    ASSERT_TRUE(restored.auth.has_value());
    EXPECT_EQ(restored.auth->type, "bearer");
    EXPECT_EQ(restored.auth->description, "Bearer token authentication");
    ASSERT_TRUE(restored.identity_did.has_value());
    EXPECT_EQ(*restored.identity_did, "did:euxis:test-agent-001");
    EXPECT_EQ(restored.metadata, (nlohmann::json{{"env", "test"}}));
}

// ---------------------------------------------------------------------------
// JSON keys are camelCase per A2A v0.2
// ---------------------------------------------------------------------------
TEST(AgentCardTest, JsonUsesCamelCase) {
    auto card = make_valid_card();
    const auto j = card.to_json();

    // Top-level camelCase keys
    EXPECT_TRUE(j.contains("identityDid"));
    EXPECT_FALSE(j.contains("identity_did"));

    // Capability camelCase keys
    ASSERT_FALSE(j["capabilities"].empty());
    const auto& cap = j["capabilities"][0];
    EXPECT_TRUE(cap.contains("inputSchema"));
    EXPECT_TRUE(cap.contains("outputSchema"));
    EXPECT_FALSE(cap.contains("input_schema"));
    EXPECT_FALSE(cap.contains("output_schema"));
}

// ---------------------------------------------------------------------------
// Optional fields are omitted when not set
// ---------------------------------------------------------------------------
TEST(AgentCardTest, OptionalFieldsOmitted) {
    AgentCard card;
    card.name = "minimal";
    card.description = "desc";
    card.url = "http://localhost";
    card.version = "0.1";

    Capability cap;
    cap.name = "noop";
    cap.description = "does nothing";
    card.capabilities.push_back(std::move(cap));

    const auto j = card.to_json();

    EXPECT_FALSE(j.contains("auth"));
    EXPECT_FALSE(j.contains("identityDid"));
    EXPECT_FALSE(j.contains("metadata"));

    // Capability should not have optional schemas
    const auto& c = j["capabilities"][0];
    EXPECT_FALSE(c.contains("inputSchema"));
    EXPECT_FALSE(c.contains("outputSchema"));
}

// ---------------------------------------------------------------------------
// validate_card: valid card passes
// ---------------------------------------------------------------------------
TEST(AgentCardTest, ValidateCardPasses) {
    const auto card = make_valid_card();
    auto result = validate_card(card);
    EXPECT_TRUE(result.has_value());
}

// ---------------------------------------------------------------------------
// validate_card: empty name fails
// ---------------------------------------------------------------------------
TEST(AgentCardTest, ValidateCardEmptyNameFails) {
    auto card = make_valid_card();
    card.name = "";
    auto result = validate_card(card);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("name"), std::string::npos);
}

// ---------------------------------------------------------------------------
// validate_card: empty url fails
// ---------------------------------------------------------------------------
TEST(AgentCardTest, ValidateCardEmptyUrlFails) {
    auto card = make_valid_card();
    card.url = "";
    auto result = validate_card(card);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("url"), std::string::npos);
}

// ---------------------------------------------------------------------------
// validate_card: no capabilities fails
// ---------------------------------------------------------------------------
TEST(AgentCardTest, ValidateCardNoCapabilitiesFails) {
    auto card = make_valid_card();
    card.capabilities.clear();
    auto result = validate_card(card);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("capability"), std::string::npos);
}

// ---------------------------------------------------------------------------
// from_json with multiple capabilities
// ---------------------------------------------------------------------------
TEST(AgentCardTest, MultipleCapabilities) {
    AgentCard card;
    card.name = "multi";
    card.description = "multi-cap agent";
    card.url = "http://example.com";
    card.version = "0.2";

    Capability cap1;
    cap1.name = "search";
    cap1.description = "Search capability";
    card.capabilities.push_back(std::move(cap1));

    Capability cap2;
    cap2.name = "summarize";
    cap2.description = "Summarization capability";
    cap2.input_schema = nlohmann::json{{"type", "object"}};
    card.capabilities.push_back(std::move(cap2));

    const auto j = card.to_json();
    const auto restored = AgentCard::from_json(j);

    ASSERT_EQ(restored.capabilities.size(), 2u);
    EXPECT_EQ(restored.capabilities[0].name, "search");
    EXPECT_EQ(restored.capabilities[1].name, "summarize");
    ASSERT_TRUE(restored.capabilities[1].input_schema.has_value());
    EXPECT_FALSE(restored.capabilities[0].input_schema.has_value());
}

// ---------------------------------------------------------------------------
// from_json with metadata field present (covers line 91)
// ---------------------------------------------------------------------------
TEST(AgentCardTest, FromJsonWithMetadata) {
    nlohmann::json j = {
        {"name", "meta-agent"},
        {"description", "Agent with metadata"},
        {"url", "http://localhost:9000"},
        {"version", "1.0.0"},
        {"capabilities", nlohmann::json::array({
            {{"name", "ping"}, {"description", "Ping capability"}}
        })},
        {"metadata", {{"env", "production"}, {"tier", "premium"}}},
    };

    auto card = AgentCard::from_json(j);
    EXPECT_EQ(card.name, "meta-agent");
    EXPECT_FALSE(card.metadata.is_null());
    EXPECT_EQ(card.metadata["env"], "production");
    EXPECT_EQ(card.metadata["tier"], "premium");
}

} // namespace
} // namespace euxis::a2a
