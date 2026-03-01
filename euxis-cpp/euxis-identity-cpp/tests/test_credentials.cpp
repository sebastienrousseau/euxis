#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/identity/credentials.hpp"

namespace euxis::identity {
namespace {

class CredentialsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }

    void SetUp() override {
        randombytes_buf(issuer_key_.data(), issuer_key_.size());
        randombytes_buf(wrong_key_.data(), wrong_key_.size());
    }

    std::array<std::byte, 32> issuer_key_{};
    std::array<std::byte, 32> wrong_key_{};
};

// --- issue_credential + verify_credential roundtrip ---

TEST_F(CredentialsTest, IssueAndVerifyRoundtrip) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    EXPECT_FALSE(cred.id.empty());
    EXPECT_EQ(cred.issuer_did, "did:euxis:issuer");
    EXPECT_EQ(cred.subject_did, "did:euxis:subject");
    ASSERT_EQ(cred.claims.size(), 1);
    EXPECT_EQ(cred.claims[0].type, "role");
    EXPECT_EQ(cred.claims[0].value, "admin");
    EXPECT_EQ(cred.claims[0].scope, "system");
    EXPECT_EQ(cred.proof.type, "HmacSha256Signature2026");
    EXPECT_FALSE(cred.proof.proof_value.empty());
    EXPECT_FALSE(cred.issued_at.empty());
    EXPECT_FALSE(cred.expires_at.empty());

    EXPECT_TRUE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, VerifyWithWrongKeyFails) {
    std::vector<Claim> claims = {
        {.type = "capability", .value = "read", .scope = "files"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    // Verification with the wrong key must fail
    EXPECT_FALSE(verify_credential(cred, wrong_key_));
}

TEST_F(CredentialsTest, TamperedClaimFailsVerification) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    // Tamper with the claim
    cred.claims[0].value = "guest";

    EXPECT_FALSE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, MultipleClaims) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "operator", .scope = "cluster"},
        {.type = "capability", .value = "deploy", .scope = "production"},
        {.type = "level", .value = "senior", .scope = "org"},
    };

    const auto cred = issue_credential(
        "did:euxis:multi-issuer", issuer_key_, "did:euxis:multi-subject", claims);

    ASSERT_EQ(cred.claims.size(), 3);
    EXPECT_EQ(cred.claims[0].type, "role");
    EXPECT_EQ(cred.claims[1].type, "capability");
    EXPECT_EQ(cred.claims[2].type, "level");

    EXPECT_TRUE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, EmptyClaims) {
    std::vector<Claim> claims = {};

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    EXPECT_TRUE(cred.claims.empty());
    EXPECT_TRUE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, CustomTTL) {
    std::vector<Claim> claims = {
        {.type = "access", .value = "read", .scope = "data"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims,
        std::chrono::seconds{86400}  // 24 hours
    );

    // We cannot test the exact expiry time but we can verify it's different
    // from the issued_at (since TTL > 0).
    EXPECT_NE(cred.issued_at, cred.expires_at);
    EXPECT_TRUE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, CredentialIdIsUnique) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "agent", .scope = "mesh"},
    };

    const auto cred1 = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);
    const auto cred2 = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    EXPECT_NE(cred1.id, cred2.id);
}

TEST_F(CredentialsTest, ProofVerificationMethod) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "agent", .scope = "mesh"},
    };

    const auto cred = issue_credential(
        "did:euxis:my-issuer", issuer_key_, "did:euxis:subject", claims);

    EXPECT_EQ(cred.proof.verification_method, "did:euxis:my-issuer#keys-1");
    EXPECT_EQ(cred.proof.created, cred.issued_at);
}

// --- to_json ---

TEST_F(CredentialsTest, ToJsonContainsAllFields) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);
    const auto j = cred.to_json();

    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("issuer"));
    EXPECT_TRUE(j.contains("subject"));
    EXPECT_TRUE(j.contains("claims"));
    EXPECT_TRUE(j.contains("proof"));
    EXPECT_TRUE(j.contains("issuedAt"));
    EXPECT_TRUE(j.contains("expiresAt"));

    EXPECT_EQ(j["issuer"], "did:euxis:issuer");
    EXPECT_EQ(j["subject"], "did:euxis:subject");
    ASSERT_EQ(j["claims"].size(), 1);
    EXPECT_EQ(j["claims"][0]["type"], "role");
    EXPECT_EQ(j["claims"][0]["value"], "admin");

    EXPECT_EQ(j["proof"]["type"], "HmacSha256Signature2026");
    EXPECT_TRUE(j["proof"].contains("proofValue"));
    EXPECT_TRUE(j["proof"].contains("verificationMethod"));
}

TEST_F(CredentialsTest, ToJsonSerializesAndParses) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);
    const auto j = cred.to_json();
    const auto serialized = j.dump();
    const auto parsed = nlohmann::json::parse(serialized);

    EXPECT_EQ(parsed, j);
}

TEST_F(CredentialsTest, TamperedProofValueFails) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    // Replace last hex character to tamper the proof
    auto& pv = cred.proof.proof_value;
    const char last = pv.back();
    pv.back() = (last == 'a') ? 'b' : 'a';

    EXPECT_FALSE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, NonHexProofValueFails) {
    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    auto cred = issue_credential(
        "did:euxis:issuer", issuer_key_, "did:euxis:subject", claims);

    // Set proof to invalid hex
    cred.proof.proof_value = "not-hex-at-all";

    EXPECT_FALSE(verify_credential(cred, issuer_key_));
}

TEST_F(CredentialsTest, NonStandardKeySize) {
    // Test with a key that is not exactly 32 bytes (triggers generichash path)
    std::array<std::byte, 64> long_key{};
    randombytes_buf(long_key.data(), long_key.size());

    std::vector<Claim> claims = {
        {.type = "role", .value = "admin", .scope = "system"},
    };

    const auto cred = issue_credential(
        "did:euxis:issuer",
        std::span<const std::byte>{long_key.data(), long_key.size()},
        "did:euxis:subject",
        claims
    );

    EXPECT_TRUE(verify_credential(
        cred, std::span<const std::byte>{long_key.data(), long_key.size()}));
    EXPECT_FALSE(verify_credential(cred, issuer_key_));
}

} // namespace
} // namespace euxis::identity
