#include <gtest/gtest.h>
#include <sodium.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/errors.hpp"
#include "euxis/crypto/types.hpp"

namespace euxis::crypto {
namespace {

// ---------------------------------------------------------------------------
// Fixture: initialise libsodium once
// ---------------------------------------------------------------------------
class AesGcmTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    /// Helper: generate a random 32-byte key.
    static auto random_key() -> std::array<std::byte, 32> {
        std::array<std::byte, 32> key{};
        randombytes_buf(key.data(), key.size());
        return key;
    }

    /// Helper: make a byte span from a string literal.
    static auto as_bytes(const std::string& s) -> std::vector<std::byte> {
        std::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }
};

// ---------------------------------------------------------------------------
// Basic encrypt / decrypt roundtrip
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, EncryptDecryptRoundtrip) {
    const auto key = random_key();
    const auto plaintext = as_bytes("Hello, Euxis crypto world!");

    auto enc = encrypt(plaintext, key);
    ASSERT_TRUE(enc.has_value()) << to_string(enc.error());

    EXPECT_EQ(enc->algorithm, "AES-256-GCM");
    // ciphertext must be longer than plaintext (16-byte tag)
    EXPECT_EQ(enc->ciphertext.size(), plaintext.size() + 16);

    auto dec = decrypt(enc->ciphertext, key, enc->iv);
    ASSERT_TRUE(dec.has_value()) << to_string(dec.error());

    EXPECT_EQ(dec->plaintext, plaintext);
    EXPECT_EQ(dec->algorithm, "AES-256-GCM");
    EXPECT_EQ(dec->to_string(), "Hello, Euxis crypto world!");
}

// ---------------------------------------------------------------------------
// Decryption with the wrong key must fail
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, WrongKeyFails) {
    const auto key1 = random_key();
    const auto key2 = random_key();
    const auto plaintext = as_bytes("secret data");

    auto enc = encrypt(plaintext, key1);
    ASSERT_TRUE(enc.has_value());

    auto dec = decrypt(enc->ciphertext, key2, enc->iv);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// ---------------------------------------------------------------------------
// Empty plaintext roundtrip
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, EmptyData) {
    const auto key = random_key();
    const std::vector<std::byte> empty;

    auto enc = encrypt(empty, key);
    ASSERT_TRUE(enc.has_value());
    // Ciphertext is just the 16-byte auth tag
    EXPECT_EQ(enc->ciphertext.size(), 16u);

    auto dec = decrypt(enc->ciphertext, key, enc->iv);
    ASSERT_TRUE(dec.has_value());
    EXPECT_TRUE(dec->plaintext.empty());
}

// ---------------------------------------------------------------------------
// Large data roundtrip (1 MB)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, LargeData) {
    const auto key = random_key();
    constexpr size_t size = 1024 * 1024; // 1 MB
    std::vector<std::byte> big(size);
    randombytes_buf(big.data(), big.size());

    auto enc = encrypt(big, key);
    ASSERT_TRUE(enc.has_value());
    EXPECT_EQ(enc->ciphertext.size(), size + 16);

    auto dec = decrypt(enc->ciphertext, key, enc->iv);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->plaintext, big);
}

// ---------------------------------------------------------------------------
// to_base64 produces non-empty output that differs from raw
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, ToBase64) {
    const auto key = random_key();
    const auto plaintext = as_bytes("base64 test");

    auto enc = encrypt(plaintext, key);
    ASSERT_TRUE(enc.has_value());

    const auto b64 = enc->to_base64();
    EXPECT_FALSE(b64.empty());
    // Base64 uses only printable ASCII characters
    for (char c : b64) {
        EXPECT_GE(c, '+');  // lowest valid Base64 char
        EXPECT_LE(c, 'z');  // highest valid Base64 char
    }
}

// ---------------------------------------------------------------------------
// Tampered ciphertext fails authentication
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, TamperedCiphertextFails) {
    const auto key = random_key();
    const auto plaintext = as_bytes("tamper test");

    auto enc = encrypt(plaintext, key);
    ASSERT_TRUE(enc.has_value());

    // Flip a byte in the ciphertext
    auto tampered = enc->ciphertext;
    tampered[0] = static_cast<std::byte>(
        static_cast<uint8_t>(tampered[0]) ^ 0xFF);

    auto dec = decrypt(tampered, key, enc->iv);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// ---------------------------------------------------------------------------
// Each encryption produces a unique IV (probabilistic but near-certain)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, UniqueIVs) {
    const auto key = random_key();
    const auto plaintext = as_bytes("iv uniqueness");

    auto enc1 = encrypt(plaintext, key);
    auto enc2 = encrypt(plaintext, key);
    ASSERT_TRUE(enc1.has_value());
    ASSERT_TRUE(enc2.has_value());

    EXPECT_NE(enc1->iv, enc2->iv);
    // Ciphertexts should also differ due to different IVs
    EXPECT_NE(enc1->ciphertext, enc2->ciphertext);
}

} // namespace
} // namespace euxis::crypto
