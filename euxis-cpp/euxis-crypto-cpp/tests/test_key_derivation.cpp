#include <gtest/gtest.h>
#include <sodium.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "euxis/crypto/errors.hpp"
#include "euxis/crypto/key_derivation.hpp"

namespace euxis::crypto {
namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class KeyDerivationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    static auto as_bytes(const std::string& s) -> std::vector<std::byte> {
        std::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }

    /// Generate a random salt of exactly crypto_pwhash_SALTBYTES (16).
    static auto random_salt() -> std::vector<std::byte> {
        std::vector<std::byte> salt(crypto_pwhash_SALTBYTES);
        randombytes_buf(salt.data(), salt.size());
        return salt;
    }
};

// ---------------------------------------------------------------------------
// Same password + same salt => same derived key (deterministic)
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, SameSaltSameKey) {
    const auto password = as_bytes("my-secret-password");
    const auto salt = random_salt();

    // Use low iterations for speed in tests
    auto dk1 = derive_key(password, salt, /*iterations=*/1);
    auto dk2 = derive_key(password, salt, /*iterations=*/1);
    ASSERT_TRUE(dk1.has_value()) << to_string(dk1.error());
    ASSERT_TRUE(dk2.has_value()) << to_string(dk2.error());

    EXPECT_EQ(dk1->key, dk2->key);
    EXPECT_EQ(dk1->salt, dk2->salt);
}

// ---------------------------------------------------------------------------
// Different salt => different derived key
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, DifferentSaltDifferentKey) {
    const auto password = as_bytes("my-secret-password");
    const auto salt1 = random_salt();
    const auto salt2 = random_salt();

    auto dk1 = derive_key(password, salt1, /*iterations=*/1);
    auto dk2 = derive_key(password, salt2, /*iterations=*/1);
    ASSERT_TRUE(dk1.has_value());
    ASSERT_TRUE(dk2.has_value());

    EXPECT_NE(dk1->key, dk2->key);
}

// ---------------------------------------------------------------------------
// generate_key produces the requested size
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, GenerateKeyDefaultSize) {
    auto key = generate_key();
    EXPECT_EQ(key.size(), 32u);
}

TEST_F(KeyDerivationTest, GenerateKeyCustomSize) {
    auto key16 = generate_key(16);
    EXPECT_EQ(key16.size(), 16u);

    auto key64 = generate_key(64);
    EXPECT_EQ(key64.size(), 64u);
}

// ---------------------------------------------------------------------------
// generate_key produces different keys each call
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, GenerateKeyUnique) {
    auto k1 = generate_key();
    auto k2 = generate_key();
    EXPECT_NE(k1, k2);
}

// ---------------------------------------------------------------------------
// Invalid salt size is rejected
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, InvalidSaltSizeFails) {
    const auto password = as_bytes("pw");
    const std::vector<std::byte> bad_salt(8); // too short

    auto dk = derive_key(password, bad_salt);
    ASSERT_FALSE(dk.has_value());
    EXPECT_EQ(dk.error(), CryptoError::KeyDerivationFailed);
}

// ---------------------------------------------------------------------------
// Custom key size
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, CustomKeySize) {
    const auto password = as_bytes("password");
    const auto salt = random_salt();

    auto dk = derive_key(password, salt, /*iterations=*/1, /*key_size=*/64);
    ASSERT_TRUE(dk.has_value()) << to_string(dk.error());
    EXPECT_EQ(dk->key.size(), 64u);
}

// ---------------------------------------------------------------------------
// Derived key salt matches the input salt
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, ReturnedSaltMatchesInput) {
    const auto password = as_bytes("pw");
    const auto salt = random_salt();

    auto dk = derive_key(password, salt, /*iterations=*/1);
    ASSERT_TRUE(dk.has_value());
    EXPECT_EQ(dk->salt.size(), salt.size());
    EXPECT_EQ(dk->salt, std::vector<std::byte>(salt.begin(), salt.end()));
}

// ---------------------------------------------------------------------------
// High iterations (>= 100,000) uses SENSITIVE opslimit
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, HighIterationsSensitive) {
    const auto password = as_bytes("strong-password");
    const auto salt = random_salt();

    // 100000 iterations -> SENSITIVE opslimit
    auto dk = derive_key(password, salt, /*iterations=*/100000);
    ASSERT_TRUE(dk.has_value()) << to_string(dk.error());
    EXPECT_EQ(dk->key.size(), 32u);
}

// ---------------------------------------------------------------------------
// Moderate iterations (>= 10,000) uses MODERATE opslimit
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, ModerateIterations) {
    const auto password = as_bytes("moderate-password");
    const auto salt = random_salt();

    auto dk = derive_key(password, salt, /*iterations=*/10000);
    ASSERT_TRUE(dk.has_value()) << to_string(dk.error());
    EXPECT_EQ(dk->key.size(), 32u);
}

// ---------------------------------------------------------------------------
// Invalid key size (too small) is rejected
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, KeySizeTooSmall) {
    const auto password = as_bytes("pw");
    const auto salt = random_salt();

    // crypto_pwhash_BYTES_MIN is typically 16; try 0
    auto dk = derive_key(password, salt, /*iterations=*/1, /*key_size=*/0);
    ASSERT_FALSE(dk.has_value());
    EXPECT_EQ(dk.error(), CryptoError::KeyDerivationFailed);
}

// ---------------------------------------------------------------------------
// Empty password still works
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, EmptyPasswordWorks) {
    const auto password = as_bytes("");
    const auto salt = random_salt();

    auto dk = derive_key(password, salt, /*iterations=*/1);
    ASSERT_TRUE(dk.has_value()) << to_string(dk.error());
    EXPECT_EQ(dk->key.size(), 32u);
}

// ---------------------------------------------------------------------------
// Salt too long is rejected
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, SaltTooLong) {
    const auto password = as_bytes("pw");
    const std::vector<std::byte> long_salt(32);  // 32 bytes, expected 16

    auto dk = derive_key(password, long_salt);
    ASSERT_FALSE(dk.has_value());
    EXPECT_EQ(dk.error(), CryptoError::KeyDerivationFailed);
}

// ---------------------------------------------------------------------------
// Different iterations produce same key (opslimit maps, not directly used)
// ---------------------------------------------------------------------------
TEST_F(KeyDerivationTest, SameOpslimitBucketSameKey) {
    const auto password = as_bytes("test-password");
    const auto salt = random_salt();

    // Both < 10000 -> INTERACTIVE
    auto dk1 = derive_key(password, salt, /*iterations=*/1);
    auto dk2 = derive_key(password, salt, /*iterations=*/5000);
    ASSERT_TRUE(dk1.has_value());
    ASSERT_TRUE(dk2.has_value());
    // Both use the same opslimit (INTERACTIVE) and memlimit, so keys should match
    EXPECT_EQ(dk1->key, dk2->key);
}

} // namespace
} // namespace euxis::crypto
