#include <gtest/gtest.h>
#include <sodium.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "euxis/crypto/ed25519.hpp"

namespace euxis::crypto {
namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class Ed25519Test : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    static auto as_bytes(const std::string& s) -> std::vector<std::byte> {
        std::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }
};

// ---------------------------------------------------------------------------
// Sign / verify roundtrip
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, SignVerifyRoundtrip) {
    auto kp = generate_keypair();
    const auto msg = as_bytes("Euxis signed message");

    auto sig = sign(kp.secret_key, msg);
    EXPECT_TRUE(verify(kp.public_key, msg, sig));
}

// ---------------------------------------------------------------------------
// Wrong public key fails verification
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, WrongPublicKeyFails) {
    auto kp1 = generate_keypair();
    auto kp2 = generate_keypair();
    const auto msg = as_bytes("test message");

    auto sig = sign(kp1.secret_key, msg);
    EXPECT_FALSE(verify(kp2.public_key, msg, sig));
}

// ---------------------------------------------------------------------------
// Wrong message fails verification
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, WrongMessageFails) {
    auto kp = generate_keypair();
    const auto msg1 = as_bytes("original message");
    const auto msg2 = as_bytes("tampered message");

    auto sig = sign(kp.secret_key, msg1);
    EXPECT_FALSE(verify(kp.public_key, msg2, sig));
}

// ---------------------------------------------------------------------------
// Different keypairs produce different signatures
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, DifferentKeypairsDifferentSignatures) {
    auto kp1 = generate_keypair();
    auto kp2 = generate_keypair();
    const auto msg = as_bytes("same message");

    auto sig1 = sign(kp1.secret_key, msg);
    auto sig2 = sign(kp2.secret_key, msg);

    EXPECT_NE(sig1, sig2);

    // Each signature only valid with its own keypair
    EXPECT_TRUE(verify(kp1.public_key, msg, sig1));
    EXPECT_TRUE(verify(kp2.public_key, msg, sig2));
    EXPECT_FALSE(verify(kp1.public_key, msg, sig2));
    EXPECT_FALSE(verify(kp2.public_key, msg, sig1));
}

// ---------------------------------------------------------------------------
// Empty message can be signed and verified
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, EmptyMessage) {
    auto kp = generate_keypair();
    const std::vector<std::byte> empty;

    auto sig = sign(kp.secret_key, empty);
    EXPECT_TRUE(verify(kp.public_key, empty, sig));
}

// ---------------------------------------------------------------------------
// Large message sign / verify
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, LargeMessage) {
    auto kp = generate_keypair();
    std::vector<std::byte> big(256 * 1024); // 256 KB
    randombytes_buf(big.data(), big.size());

    auto sig = sign(kp.secret_key, big);
    EXPECT_TRUE(verify(kp.public_key, big, sig));
}

// ---------------------------------------------------------------------------
// Tampered signature fails
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, TamperedSignatureFails) {
    auto kp = generate_keypair();
    const auto msg = as_bytes("integrity test");

    auto sig = sign(kp.secret_key, msg);
    // Flip a byte in the signature
    sig[0] = static_cast<std::byte>(static_cast<uint8_t>(sig[0]) ^ 0xFF);
    EXPECT_FALSE(verify(kp.public_key, msg, sig));
}

// ---------------------------------------------------------------------------
// Keypair fields have correct sizes
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, KeypairSizes) {
    auto kp = generate_keypair();
    EXPECT_EQ(kp.secret_key.size(), 64u);
    EXPECT_EQ(kp.public_key.size(), 32u);
}

// ---------------------------------------------------------------------------
// Multiple keypairs are distinct
// ---------------------------------------------------------------------------
TEST_F(Ed25519Test, KeypairsAreDistinct) {
    auto kp1 = generate_keypair();
    auto kp2 = generate_keypair();
    EXPECT_NE(kp1.public_key, kp2.public_key);
    EXPECT_NE(kp1.secret_key, kp2.secret_key);
}

} // namespace
} // namespace euxis::crypto
