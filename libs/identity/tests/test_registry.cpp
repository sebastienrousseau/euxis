#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/identity/registry.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::identity {
namespace {

class RegistryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }

    void SetUp() override {
        registry_ = std::make_unique<InMemoryIdentityRegistry>();
    }

    /// Helper to create a minimal AgentIdentity for testing.
    [[nodiscard]] auto make_identity(std::string did) -> AgentIdentity {
        std::array<std::byte, 32> pub_key{};
        randombytes_buf(pub_key.data(), pub_key.size());
        return AgentIdentity{
            .did = std::move(did),
            .name = "Test Agent",
            .role = "tester",
            .public_key = {reinterpret_cast<char*>(pub_key.data()), pub_key.size()},
            .credentials = {},
            .attestations = {},
            .created_at = "2026-03-01T00:00:00Z",
            .metadata = nlohmann::json{{"version", "1.0"}},
        };
    }

    std::unique_ptr<InMemoryIdentityRegistry> registry_;
};

// --- register_agent ---

TEST_F(RegistryTest, RegisterAgentSuccess) {
    auto id = make_identity("did:euxis:agent-1");
    EXPECT_TRUE(registry_->register_agent(std::move(id)));
}

TEST_F(RegistryTest, RegisterDuplicateFails) {
    auto id1 = make_identity("did:euxis:agent-dup");
    auto id2 = make_identity("did:euxis:agent-dup");

    EXPECT_TRUE(registry_->register_agent(std::move(id1)));
    EXPECT_FALSE(registry_->register_agent(std::move(id2)));
}

TEST_F(RegistryTest, RegisterMultipleAgents) {
    EXPECT_TRUE(registry_->register_agent(make_identity("did:euxis:a")));
    EXPECT_TRUE(registry_->register_agent(make_identity("did:euxis:b")));
    EXPECT_TRUE(registry_->register_agent(make_identity("did:euxis:c")));
}

// --- resolve ---

TEST_F(RegistryTest, ResolveRegisteredAgent) {
    auto id = make_identity("did:euxis:resolve-me");
    registry_->register_agent(std::move(id));

    const auto result = registry_->resolve("did:euxis:resolve-me");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->did, "did:euxis:resolve-me");
    EXPECT_EQ(result->public_key.size(), 32);
    EXPECT_EQ(result->metadata["version"], "1.0");
}

TEST_F(RegistryTest, ResolveUnknownDID) {
    const auto result = registry_->resolve("did:euxis:nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(RegistryTest, ResolveAfterMultipleRegistrations) {
    registry_->register_agent(make_identity("did:euxis:x"));
    registry_->register_agent(make_identity("did:euxis:y"));
    registry_->register_agent(make_identity("did:euxis:z"));

    EXPECT_TRUE(registry_->resolve("did:euxis:x").has_value());
    EXPECT_TRUE(registry_->resolve("did:euxis:y").has_value());
    EXPECT_TRUE(registry_->resolve("did:euxis:z").has_value());
    EXPECT_FALSE(registry_->resolve("did:euxis:w").has_value());
}

// --- list_agents ---

TEST_F(RegistryTest, ListAgentsEmpty) {
    const auto agents = registry_->list_agents();
    EXPECT_TRUE(agents.empty());
}

TEST_F(RegistryTest, ListAgentsAfterRegistrations) {
    registry_->register_agent(make_identity("did:euxis:l1"));
    registry_->register_agent(make_identity("did:euxis:l2"));
    registry_->register_agent(make_identity("did:euxis:l3"));

    const auto agents = registry_->list_agents();
    ASSERT_EQ(agents.size(), 3);

    // Collect DIDs into a set for order-independent comparison
    std::set<std::string> dids;
    for (const auto& a : agents) {
        dids.insert(a.did);
    }
    EXPECT_TRUE(dids.contains("did:euxis:l1"));
    EXPECT_TRUE(dids.contains("did:euxis:l2"));
    EXPECT_TRUE(dids.contains("did:euxis:l3"));
}

// --- revoke ---

TEST_F(RegistryTest, RevokeExistingAgent) {
    registry_->register_agent(make_identity("did:euxis:revoke-me"));

    EXPECT_TRUE(registry_->revoke("did:euxis:revoke-me"));
    EXPECT_FALSE(registry_->resolve("did:euxis:revoke-me").has_value());
}

TEST_F(RegistryTest, RevokeNonexistentAgent) {
    EXPECT_FALSE(registry_->revoke("did:euxis:ghost"));
}

TEST_F(RegistryTest, RevokeDoesNotAffectOthers) {
    registry_->register_agent(make_identity("did:euxis:keep"));
    registry_->register_agent(make_identity("did:euxis:remove"));

    EXPECT_TRUE(registry_->revoke("did:euxis:remove"));
    EXPECT_TRUE(registry_->resolve("did:euxis:keep").has_value());
    EXPECT_FALSE(registry_->resolve("did:euxis:remove").has_value());
    EXPECT_EQ(registry_->list_agents().size(), 1);
}

TEST_F(RegistryTest, RevokeAndReRegister) {
    registry_->register_agent(make_identity("did:euxis:cycle"));
    EXPECT_TRUE(registry_->revoke("did:euxis:cycle"));
    EXPECT_FALSE(registry_->resolve("did:euxis:cycle").has_value());

    // Re-register the same DID should succeed
    EXPECT_TRUE(registry_->register_agent(make_identity("did:euxis:cycle")));
    EXPECT_TRUE(registry_->resolve("did:euxis:cycle").has_value());
}

TEST_F(RegistryTest, DoubleRevokeFails) {
    registry_->register_agent(make_identity("did:euxis:double-rev"));
    EXPECT_TRUE(registry_->revoke("did:euxis:double-rev"));
    EXPECT_FALSE(registry_->revoke("did:euxis:double-rev"));
}

// --- identity data preservation ---

TEST_F(RegistryTest, PreservesCredentialsAndAttestations) {
    auto id = make_identity("did:euxis:full-agent");

    // Add a credential
    VerifiableCredential cred;
    cred.id = "cred-001";
    cred.issuer_did = "did:euxis:issuer";
    cred.subject_did = "did:euxis:full-agent";
    cred.claims = {{.type = "role", .value = "admin", .scope = "system"}};
    id.credentials.push_back(std::move(cred));

    // Add an attestation
    Attestation att;
    att.id = "att-001";
    att.attester_did = "did:euxis:peer";
    att.subject_did = "did:euxis:full-agent";
    att.attestation_type = "trust";
    att.confidence = 0.9;
    id.attestations.push_back(std::move(att));

    registry_->register_agent(std::move(id));

    const auto resolved = registry_->resolve("did:euxis:full-agent");
    ASSERT_TRUE(resolved.has_value());
    ASSERT_EQ(resolved->credentials.size(), 1);
    EXPECT_EQ(resolved->credentials[0].id, "cred-001");
    ASSERT_EQ(resolved->attestations.size(), 1);
    EXPECT_EQ(resolved->attestations[0].id, "att-001");
    EXPECT_DOUBLE_EQ(resolved->attestations[0].confidence, 0.9);
}

TEST_F(RegistryTest, RegisterDuplicateReturnsFalseWithExistingData) {
    // Exercises the duplicate registration path (line 34: agents_.contains(did))
    auto id1 = make_identity("did:euxis:dup-check");
    auto id2 = make_identity("did:euxis:dup-check");

    EXPECT_TRUE(registry_->register_agent(std::move(id1)));
    // Second registration with same DID must fail
    EXPECT_FALSE(registry_->register_agent(std::move(id2)));
    // Original registration should still be accessible
    auto resolved = registry_->resolve("did:euxis:dup-check");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->did, "did:euxis:dup-check");
}

} // namespace
} // namespace euxis::identity

// NOLINTEND(bugprone-unchecked-optional-access)
