#include <gtest/gtest.h>

#include "euxis/attest/bundle.hpp"

namespace euxis::attest {
namespace {

Bundle make_minimal_bundle() {
    Bundle b;
    b.public_key.hint = std::string(64, 'a');
    b.dsse_envelope.payload_type = "application/vnd.in-toto+json";
    b.dsse_envelope.payload      = "e30=";  // base64 of "{}"
    b.dsse_envelope.signatures.push_back({.keyid = "k", .sig = "ZGVm"});
    return b;
}

TEST(Bundle, JsonHasMediaTypeAndVerificationMaterial) {
    auto j = to_json(make_minimal_bundle());
    EXPECT_EQ(j["mediaType"], "application/vnd.dev.sigstore.bundle.v0.3+json");
    ASSERT_TRUE(j.contains("verificationMaterial"));
    ASSERT_TRUE(j["verificationMaterial"].contains("publicKey"));
    EXPECT_EQ(j["verificationMaterial"]["publicKey"]["hint"],
              std::string(64, 'a'));
    ASSERT_TRUE(j.contains("dsseEnvelope"));
}

TEST(Bundle, RawBytesOnlyEmittedWhenPresent) {
    auto j = to_json(make_minimal_bundle());
    EXPECT_FALSE(j["verificationMaterial"]["publicKey"].contains("rawBytes"));

    auto b = make_minimal_bundle();
    b.public_key.raw_b64 = "ZGVmZ2hp";
    auto j2 = to_json(b);
    EXPECT_EQ(j2["verificationMaterial"]["publicKey"]["rawBytes"], "ZGVmZ2hp");
}

TEST(Bundle, TlogEntriesEmittedAndRoundTrip) {
    auto b = make_minimal_bundle();
    TlogEntry t;
    t.log_index            = 42;
    t.integrated_time_unix = 1700000000;
    t.log_id               = "rekor-v2-public";
    t.kind_version_kind    = "dsse";
    t.kind_version_version = "0.0.1";
    t.inclusion_proof = {{"logIndex", 42}, {"hashes", nlohmann::json::array()}};
    b.tlog_entries.push_back(t);

    auto j = to_json(b);
    ASSERT_TRUE(j["verificationMaterial"].contains("tlogEntries"));
    EXPECT_EQ(j["verificationMaterial"]["tlogEntries"][0]["logIndex"], 42);
    EXPECT_EQ(j["verificationMaterial"]["tlogEntries"][0]["kindVersion"]["kind"], "dsse");

    auto parsed = from_json(j);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->tlog_entries.size(), 1U);
    EXPECT_EQ(parsed->tlog_entries[0].log_index, 42);
}

TEST(Bundle, FromJsonRoundTripPreservesEnvelope) {
    auto b = make_minimal_bundle();
    auto j = to_json(b);
    auto parsed = from_json(j);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->dsse_envelope.payload_type,
              "application/vnd.in-toto+json");
    ASSERT_EQ(parsed->dsse_envelope.signatures.size(), 1U);
    EXPECT_EQ(parsed->dsse_envelope.signatures[0].keyid, "k");
}

TEST(Bundle, FromJsonRejectsMissingVerificationMaterial) {
    nlohmann::json j = {{"dsseEnvelope", nlohmann::json::object()}};
    EXPECT_FALSE(from_json(j).has_value());
}

TEST(Bundle, FromJsonRejectsMissingEnvelope) {
    nlohmann::json j = {
        {"verificationMaterial", {{"publicKey", {{"hint", "x"}}}}},
    };
    EXPECT_FALSE(from_json(j).has_value());
}

} // namespace
} // namespace euxis::attest
