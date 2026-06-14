#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "euxis/attest/dsse.hpp"
#include "euxis/attest/signing.hpp"
#include "euxis/attest/statement.hpp"
#include "euxis/crypto/ed25519.hpp"

namespace euxis::attest {
namespace {

Statement make_test_statement() {
    Statement s;
    Subject subj;
    subj.name = "test-evidence.tar.gz";
    subj.digest["sha256"] = std::string(64, 'a');
    s.subjects.push_back(subj);
    s.predicate_type = "https://slsa.dev/provenance/v1.2";
    s.predicate = {{"buildDefinition", {{"buildType", "x"}}}};
    return s;
}

TEST(Signing, KeyidIsHexAndStable) {
    auto kp = euxis::crypto::generate_keypair();
    auto a = derive_keyid(kp.public_key);
    auto b = derive_keyid(kp.public_key);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.size(), 64U);
}

TEST(Signing, SignThenVerifyRoundTrips) {
    auto kp = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto env = sign_statement(stmt, kp.secret_key,
                              derive_keyid(kp.public_key));
    ASSERT_TRUE(env.has_value());
    ASSERT_EQ(env->signatures.size(), 1U);
    EXPECT_FALSE(env->signatures[0].sig.empty());

    auto verified = verify_envelope(*env, kp.public_key);
    ASSERT_TRUE(verified.has_value());
    EXPECT_FALSE(verified->empty());
}

TEST(Signing, VerifyFailsWithWrongKey) {
    auto signer = euxis::crypto::generate_keypair();
    auto other  = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto env = sign_statement(stmt, signer.secret_key,
                              derive_keyid(signer.public_key));
    ASSERT_TRUE(env.has_value());

    auto verified = verify_envelope(*env, other.public_key);
    EXPECT_FALSE(verified.has_value());
}

TEST(Signing, VerifyFailsWhenPayloadTampered) {
    auto kp = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto env = sign_statement(stmt, kp.secret_key,
                              derive_keyid(kp.public_key));
    ASSERT_TRUE(env.has_value());

    // Mutate the encoded payload — flip one byte after decode.
    auto raw = base64_decode(env->payload);
    ASSERT_TRUE(raw.has_value());
    if (!raw->empty()) {
        raw->front() = static_cast<std::byte>(
            static_cast<unsigned char>(raw->front()) ^ 0x80);
    }
    std::string tampered_payload(
        reinterpret_cast<const char*>(raw->data()), raw->size());
    env->payload = base64_encode(tampered_payload);

    auto verified = verify_envelope(*env, kp.public_key);
    EXPECT_FALSE(verified.has_value());
}

TEST(Signing, SignRejectsInvalidStatement) {
    auto kp = euxis::crypto::generate_keypair();
    Statement empty;  // no subjects, no predicate
    auto env = sign_statement(empty, kp.secret_key, "keyid");
    EXPECT_FALSE(env.has_value());
}

TEST(Signing, EnvelopeHasInTotoPayloadType) {
    auto kp = euxis::crypto::generate_keypair();
    auto env = sign_statement(make_test_statement(), kp.secret_key,
                              derive_keyid(kp.public_key));
    ASSERT_TRUE(env.has_value());
    EXPECT_EQ(env->payload_type, "application/vnd.in-toto+json");
}

TEST(Signing, EnvelopePayloadDecodesToCanonicalStatement) {
    auto kp = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto env = sign_statement(stmt, kp.secret_key,
                              derive_keyid(kp.public_key));
    ASSERT_TRUE(env.has_value());

    auto raw = base64_decode(env->payload);
    ASSERT_TRUE(raw.has_value());
    std::string payload(
        reinterpret_cast<const char*>(raw->data()), raw->size());
    auto parsed = nlohmann::json::parse(payload);
    EXPECT_EQ(parsed["_type"], "https://in-toto.io/Statement/v1");
    EXPECT_EQ(parsed["predicateType"], "https://slsa.dev/provenance/v1.2");
}

} // namespace
} // namespace euxis::attest
