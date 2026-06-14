#include <gtest/gtest.h>

#include "euxis/attest/dsse.hpp"

namespace euxis::attest {
namespace {

TEST(Pae, MatchesSpecExample) {
    // Spec § "PAE definition" example:
    //   pae("hello", "world") =
    //     "DSSEv1 5 hello 5 world"
    auto pae_str = pae("hello", "world");
    EXPECT_EQ(pae_str, "DSSEv1 5 hello 5 world");
}

TEST(Pae, EmptyPayloadStillEncodesLength) {
    auto pae_str = pae("type", "");
    EXPECT_EQ(pae_str, "DSSEv1 4 type 0 ");
}

TEST(Pae, UnicodeLengthIsByteCount) {
    // "héllo" is 6 bytes UTF-8 not 5.
    auto pae_str = pae("type", "héllo");
    EXPECT_EQ(pae_str, "DSSEv1 4 type 6 héllo");
}

TEST(Base64, EmptyInputProducesEmptyOutput) {
    EXPECT_EQ(base64_encode(""), "");
}

TEST(Base64, KnownVectors) {
    EXPECT_EQ(base64_encode("M"),   "TQ==");
    EXPECT_EQ(base64_encode("Ma"),  "TWE=");
    EXPECT_EQ(base64_encode("Man"), "TWFu");
    EXPECT_EQ(base64_encode("hello"), "aGVsbG8=");
}

TEST(Base64, RoundTrip) {
    std::string original = "the quick brown fox jumps over the lazy dog";
    auto encoded = base64_encode(original);
    auto decoded = base64_decode(encoded);
    ASSERT_TRUE(decoded.has_value());
    std::string round_trip(
        reinterpret_cast<const char*>(decoded->data()),
        decoded->size());
    EXPECT_EQ(round_trip, original);
}

TEST(Base64, DecodeRejectsInvalidCharacter) {
    EXPECT_FALSE(base64_decode("@!#$").has_value());
}

TEST(Envelope, RoundTripPreservesFields) {
    Envelope env;
    env.payload_type = "application/vnd.in-toto+json";
    env.payload = base64_encode(std::string{"{}"});
    env.signatures.push_back({.keyid = "abc", .sig = "ZGVm"});

    auto j = to_json(env);
    auto parsed = from_json(j);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->payload_type, "application/vnd.in-toto+json");
    EXPECT_EQ(parsed->payload,      env.payload);
    ASSERT_EQ(parsed->signatures.size(), 1U);
    EXPECT_EQ(parsed->signatures[0].keyid, "abc");
    EXPECT_EQ(parsed->signatures[0].sig,   "ZGVm");
}

TEST(Envelope, FromJsonRejectsNonObject) {
    nlohmann::json j = "not an object";
    EXPECT_FALSE(from_json(j).has_value());
}

TEST(Envelope, FromJsonRejectsMissingPayloadType) {
    nlohmann::json j = {{"payload", "abc"}};
    EXPECT_FALSE(from_json(j).has_value());
}

} // namespace
} // namespace euxis::attest
