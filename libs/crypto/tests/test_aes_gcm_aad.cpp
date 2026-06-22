#include <gtest/gtest.h>
#include <sodium.h>

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
// Fixture
// ---------------------------------------------------------------------------
class AesGcmAadTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    static auto random_key() -> std::array<std::byte, 32> {
        std::array<std::byte, 32> key{};
        randombytes_buf(key.data(), key.size());
        return key;
    }

    static auto as_bytes(const std::string& s) -> std::vector<std::byte> {
        std::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }
};

// ---------------------------------------------------------------------------
// AAD encrypt / decrypt roundtrip
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, EncryptDecryptRoundtrip) {
    const auto key = random_key();
    const auto plaintext = as_bytes("authenticated payload");
    const auto aad = as_bytes("channel-id:42");

    auto enc = encrypt_aad(plaintext, key, aad);
    ASSERT_TRUE(enc.has_value()) << to_string(enc.error());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, aad);
    ASSERT_TRUE(dec.has_value()) << to_string(dec.error());

    EXPECT_EQ(dec->plaintext, plaintext);
    EXPECT_EQ(dec->to_string(), "authenticated payload");
}

// ---------------------------------------------------------------------------
// Wrong AAD causes authentication failure
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, WrongAadFails) {
    const auto key = random_key();
    const auto plaintext = as_bytes("secret");
    const auto aad_enc = as_bytes("context-A");
    const auto aad_dec = as_bytes("context-B");

    auto enc = encrypt_aad(plaintext, key, aad_enc);
    ASSERT_TRUE(enc.has_value());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, aad_dec);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// ---------------------------------------------------------------------------
// Decrypting AAD ciphertext without AAD fails
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, MissingAadFails) {
    const auto key = random_key();
    const auto plaintext = as_bytes("data");
    const auto aad = as_bytes("some-context");

    auto enc = encrypt_aad(plaintext, key, aad);
    ASSERT_TRUE(enc.has_value());

    // Try to decrypt without AAD (using the non-AAD function)
    auto dec = decrypt(enc->ciphertext, key, enc->iv);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// ---------------------------------------------------------------------------
// Different AAD contexts produce different ciphertexts even with same key+IV
// (we can't force the same IV, but we can verify that the AAD is bound)
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, DifferentAadContexts) {
    const auto key = random_key();
    const auto plaintext = as_bytes("same plaintext");
    const auto aad1 = as_bytes("user:alice");
    const auto aad2 = as_bytes("user:bob");

    auto enc1 = encrypt_aad(plaintext, key, aad1);
    auto enc2 = encrypt_aad(plaintext, key, aad2);
    ASSERT_TRUE(enc1.has_value());
    ASSERT_TRUE(enc2.has_value());

    // Each should decrypt only with its own AAD
    auto dec1_ok = decrypt_aad(enc1->ciphertext, key, enc1->iv, aad1);
    ASSERT_TRUE(dec1_ok.has_value());
    EXPECT_EQ(dec1_ok->plaintext, plaintext);

    auto dec2_ok = decrypt_aad(enc2->ciphertext, key, enc2->iv, aad2);
    ASSERT_TRUE(dec2_ok.has_value());
    EXPECT_EQ(dec2_ok->plaintext, plaintext);

    // Cross-AAD must fail
    auto dec1_bad = decrypt_aad(enc1->ciphertext, key, enc1->iv, aad2);
    EXPECT_FALSE(dec1_bad.has_value());

    auto dec2_bad = decrypt_aad(enc2->ciphertext, key, enc2->iv, aad1);
    EXPECT_FALSE(dec2_bad.has_value());
}

// ---------------------------------------------------------------------------
// Empty AAD is valid (equivalent to no AAD)
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, EmptyAadRoundtrip) {
    const auto key = random_key();
    const auto plaintext = as_bytes("payload");
    const std::vector<std::byte> empty_aad;

    auto enc = encrypt_aad(plaintext, key, empty_aad);
    ASSERT_TRUE(enc.has_value());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, empty_aad);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->plaintext, plaintext);
}

// ---------------------------------------------------------------------------
// Large AAD roundtrip
// ---------------------------------------------------------------------------
TEST_F(AesGcmAadTest, LargeAad) {
    const auto key = random_key();
    const auto plaintext = as_bytes("small payload");

    // 64 KB of AAD
    std::vector<std::byte> large_aad(std::size_t{64} * 1024);
    randombytes_buf(large_aad.data(), large_aad.size());

    auto enc = encrypt_aad(plaintext, key, large_aad);
    ASSERT_TRUE(enc.has_value());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, large_aad);
    ASSERT_TRUE(dec.has_value());
    EXPECT_EQ(dec->plaintext, plaintext);
}

} // namespace
} // namespace euxis::crypto
