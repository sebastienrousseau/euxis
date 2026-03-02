#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/identity/attestation.hpp"

namespace euxis::identity {
namespace {

class AttestationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }
};

// --- create_attestation ---

TEST_F(AttestationTest, CreateBasicAttestation) {
    const auto att = create_attestation(
        "did:euxis:attester", "did:euxis:subject", "capability", 0.95);

    EXPECT_FALSE(att.id.empty());
    EXPECT_EQ(att.attester_did, "did:euxis:attester");
    EXPECT_EQ(att.subject_did, "did:euxis:subject");
    EXPECT_EQ(att.attestation_type, "capability");
    EXPECT_DOUBLE_EQ(att.confidence, 0.95);
    EXPECT_TRUE(att.evidence.empty());
    EXPECT_FALSE(att.created_at.empty());
}

TEST_F(AttestationTest, CreateAttestationWithEvidence) {
    const auto att = create_attestation(
        "did:euxis:attester",
        "did:euxis:subject",
        "trust",
        0.85,
        "Passed benchmark suite with 98% accuracy"
    );

    EXPECT_EQ(att.evidence, "Passed benchmark suite with 98% accuracy");
}

TEST_F(AttestationTest, AttestationIdUnique) {
    const auto att1 = create_attestation(
        "did:euxis:attester", "did:euxis:subject", "type-a", 0.5);
    const auto att2 = create_attestation(
        "did:euxis:attester", "did:euxis:subject", "type-a", 0.5);

    EXPECT_NE(att1.id, att2.id);
}

// --- confidence bounds ---

TEST_F(AttestationTest, ConfidenceZero) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "test", 0.0);
    EXPECT_DOUBLE_EQ(att.confidence, 0.0);
}

TEST_F(AttestationTest, ConfidenceOne) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "test", 1.0);
    EXPECT_DOUBLE_EQ(att.confidence, 1.0);
}

TEST_F(AttestationTest, ConfidenceBelowZeroThrows) {
    EXPECT_THROW(
        (void)create_attestation("did:euxis:a", "did:euxis:b", "test", -0.1),
        std::invalid_argument
    );
}

TEST_F(AttestationTest, ConfidenceAboveOneThrows) {
    EXPECT_THROW(
        (void)create_attestation("did:euxis:a", "did:euxis:b", "test", 1.1),
        std::invalid_argument
    );
}

TEST_F(AttestationTest, ConfidenceMidRange) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "test", 0.5);
    EXPECT_DOUBLE_EQ(att.confidence, 0.5);
}

// --- timestamp format ---

TEST_F(AttestationTest, TimestampFormat) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "test", 0.5);

    // ISO-8601 format: YYYY-MM-DDTHH:MM:SSZ (20 chars)
    EXPECT_EQ(att.created_at.size(), 20);
    EXPECT_EQ(att.created_at[4], '-');
    EXPECT_EQ(att.created_at[7], '-');
    EXPECT_EQ(att.created_at[10], 'T');
    EXPECT_EQ(att.created_at[13], ':');
    EXPECT_EQ(att.created_at[16], ':');
    EXPECT_EQ(att.created_at[19], 'Z');
}

// --- JSON serialisation ---

TEST_F(AttestationTest, ToJsonContainsAllFields) {
    const auto att = create_attestation(
        "did:euxis:attester",
        "did:euxis:subject",
        "capability",
        0.9,
        "benchmark evidence"
    );
    const auto j = att.to_json();

    EXPECT_TRUE(j.contains("id"));
    EXPECT_EQ(j["attester"], "did:euxis:attester");
    EXPECT_EQ(j["subject"], "did:euxis:subject");
    EXPECT_EQ(j["type"], "capability");
    EXPECT_DOUBLE_EQ(j["confidence"].get<double>(), 0.9);
    EXPECT_EQ(j["evidence"], "benchmark evidence");
    EXPECT_TRUE(j.contains("createdAt"));
}

TEST_F(AttestationTest, ToJsonSerializesAndParses) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "trust", 0.75, "evidence data");
    const auto j = att.to_json();
    const auto serialized = j.dump();
    const auto parsed = nlohmann::json::parse(serialized);

    EXPECT_EQ(parsed, j);
    EXPECT_EQ(parsed["type"], "trust");
    EXPECT_DOUBLE_EQ(parsed["confidence"].get<double>(), 0.75);
}

TEST_F(AttestationTest, ToJsonEmptyEvidence) {
    const auto att = create_attestation(
        "did:euxis:a", "did:euxis:b", "test", 0.5);
    const auto j = att.to_json();

    EXPECT_EQ(j["evidence"], "");
}

// --- Coverage: lines 58-60 (attestation with boundary confidence values) ---
TEST_F(AttestationTest, ConfidenceExactBoundaryValues) {
    // Exactly 0.0 and 1.0 should not throw (boundary conditions)
    auto att_zero = create_attestation("did:a", "did:b", "test", 0.0);
    EXPECT_DOUBLE_EQ(att_zero.confidence, 0.0);
    EXPECT_FALSE(att_zero.id.empty());

    auto att_one = create_attestation("did:a", "did:b", "test", 1.0);
    EXPECT_DOUBLE_EQ(att_one.confidence, 1.0);
    EXPECT_FALSE(att_one.id.empty());
}

// --- Coverage: line 63 (create_attestation all fields populated) ---
TEST_F(AttestationTest, AllFieldsPopulatedAfterCreate) {
    auto att = create_attestation(
        "did:euxis:full-attester",
        "did:euxis:full-subject",
        "reliability",
        0.99,
        "comprehensive evidence string");

    EXPECT_EQ(att.attester_did, "did:euxis:full-attester");
    EXPECT_EQ(att.subject_did, "did:euxis:full-subject");
    EXPECT_EQ(att.attestation_type, "reliability");
    EXPECT_DOUBLE_EQ(att.confidence, 0.99);
    EXPECT_EQ(att.evidence, "comprehensive evidence string");
    EXPECT_FALSE(att.created_at.empty());
    EXPECT_FALSE(att.id.empty());
}

// --- Coverage: line 68 (large negative confidence) ---
TEST_F(AttestationTest, LargeNegativeConfidenceThrows) {
    EXPECT_THROW(
        (void)create_attestation("did:a", "did:b", "test", -100.0),
        std::invalid_argument);
}

// --- Coverage: UUID format of attestation id ---
TEST_F(AttestationTest, AttestationIdFormat) {
    auto att = create_attestation("did:a", "did:b", "test", 0.5);
    // UUID format: 8-4-4-4-12 = 36 chars
    EXPECT_EQ(att.id.size(), 36u);
    EXPECT_EQ(att.id[8], '-');
    EXPECT_EQ(att.id[13], '-');
    EXPECT_EQ(att.id[18], '-');
    EXPECT_EQ(att.id[23], '-');
}

} // namespace
} // namespace euxis::identity
