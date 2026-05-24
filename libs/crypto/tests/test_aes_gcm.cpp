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

// ---------------------------------------------------------------------------
// decrypt with too-short ciphertext fails (covers line 91)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, DecryptTooShortCiphertext) {
    const auto key = random_key();
    // Ciphertext shorter than 16 bytes (the auth tag size)
    std::vector<std::byte> short_ct(10, std::byte{0x42});
    std::array<std::byte, 12> iv{};
    randombytes_buf(iv.data(), iv.size());

    auto dec = decrypt(short_ct, key, iv);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::DecryptionFailed);
}

// ---------------------------------------------------------------------------
// encrypt_aad / decrypt_aad roundtrip (covers lines 146, 163)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, EncryptDecryptAadRoundtrip) {
    const auto key = random_key();
    const auto plaintext = as_bytes("AAD protected data");
    const auto aad = as_bytes("additional-auth-data");

    auto enc = encrypt_aad(plaintext, key, aad);
    ASSERT_TRUE(enc.has_value()) << to_string(enc.error());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, aad);
    ASSERT_TRUE(dec.has_value()) << to_string(dec.error());
    EXPECT_EQ(dec->plaintext, plaintext);
    EXPECT_EQ(dec->to_string(), "AAD protected data");
}

// ---------------------------------------------------------------------------
// decrypt_aad with wrong AAD fails authentication (covers line 163)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, DecryptAadWrongAadFails) {
    const auto key = random_key();
    const auto plaintext = as_bytes("secret");
    const auto aad = as_bytes("correct-aad");
    const auto wrong_aad = as_bytes("wrong-aad");

    auto enc = encrypt_aad(plaintext, key, aad);
    ASSERT_TRUE(enc.has_value());

    auto dec = decrypt_aad(enc->ciphertext, key, enc->iv, wrong_aad);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// ---------------------------------------------------------------------------
// decrypt_aad with too-short ciphertext (covers line 163)
// ---------------------------------------------------------------------------
TEST_F(AesGcmTest, DecryptAadTooShortCiphertext) {
    const auto key = random_key();
    std::vector<std::byte> short_ct(5, std::byte{0x00});
    std::array<std::byte, 12> iv{};
    randombytes_buf(iv.data(), iv.size());
    const auto aad = as_bytes("some-aad");

    auto dec = decrypt_aad(short_ct, key, iv, aad);
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::DecryptionFailed);
}

// ===========================================================================
// AesGcmContext (precomputed key schedule)
// ===========================================================================

// Round-trip via context: encrypt and decrypt with the same context.
TEST_F(AesGcmTest, ContextEncryptDecryptRoundtrip) {
    const auto key = random_key();
    auto ctx_result = AesGcmContext::create(key);
    ASSERT_TRUE(ctx_result.has_value()) << to_string(ctx_result.error());
    const auto& ctx = *ctx_result;

    const auto plaintext = as_bytes("Hello via cached context!");
    auto enc = ctx.encrypt(plaintext);
    ASSERT_TRUE(enc.has_value()) << to_string(enc.error());
    EXPECT_EQ(enc->algorithm, "AES-256-GCM");
    EXPECT_GE(enc->ciphertext.size(), plaintext.size());

    auto dec = ctx.decrypt(enc->ciphertext, enc->iv);
    ASSERT_TRUE(dec.has_value()) << to_string(dec.error());
    EXPECT_EQ(dec->plaintext.size(), plaintext.size());
    EXPECT_TRUE(std::equal(plaintext.begin(), plaintext.end(),
                           dec->plaintext.begin()));
}

// Cross-compat: ciphertext produced via context must decrypt with the free
// function (and vice-versa). Same algorithm + key, different code path.
TEST_F(AesGcmTest, ContextInteropWithFreeFunctions) {
    const auto key = random_key();
    auto ctx = AesGcmContext::create(key).value();

    const auto plaintext = as_bytes("interop check");

    // context-encrypt → free-function decrypt
    auto ctx_enc = ctx.encrypt(plaintext);
    ASSERT_TRUE(ctx_enc.has_value());
    auto free_dec = decrypt(ctx_enc->ciphertext, key, ctx_enc->iv);
    ASSERT_TRUE(free_dec.has_value()) << to_string(free_dec.error());
    EXPECT_TRUE(std::equal(plaintext.begin(), plaintext.end(),
                           free_dec->plaintext.begin()));

    // free-function encrypt → context decrypt
    auto free_enc = encrypt(plaintext, key);
    ASSERT_TRUE(free_enc.has_value());
    auto ctx_dec = ctx.decrypt(free_enc->ciphertext, free_enc->iv);
    ASSERT_TRUE(ctx_dec.has_value()) << to_string(ctx_dec.error());
    EXPECT_TRUE(std::equal(plaintext.begin(), plaintext.end(),
                           ctx_dec->plaintext.begin()));
}

// AAD round-trip via context.
TEST_F(AesGcmTest, ContextAadRoundtrip) {
    const auto key = random_key();
    auto ctx = AesGcmContext::create(key).value();
    const auto plaintext = as_bytes("with aad");
    const auto aad = as_bytes("session=42");

    auto enc = ctx.encrypt_aad(plaintext, aad);
    ASSERT_TRUE(enc.has_value());

    auto dec = ctx.decrypt_aad(enc->ciphertext, enc->iv, aad);
    ASSERT_TRUE(dec.has_value());
    EXPECT_TRUE(std::equal(plaintext.begin(), plaintext.end(),
                           dec->plaintext.begin()));
}

// Wrong AAD must fail authentication.
TEST_F(AesGcmTest, ContextWrongAadFails) {
    const auto key = random_key();
    auto ctx = AesGcmContext::create(key).value();
    auto enc = ctx.encrypt_aad(as_bytes("payload"), as_bytes("aad-1")).value();

    auto dec = ctx.decrypt_aad(enc.ciphertext, enc.iv, as_bytes("aad-2"));
    ASSERT_FALSE(dec.has_value());
    EXPECT_EQ(dec.error(), CryptoError::AuthenticationFailed);
}

// All-zero key is rejected by libsodium safety check.
TEST_F(AesGcmTest, ContextRejectsZeroKey) {
    std::array<std::byte, 32> zero_key{};  // all zeros
    auto ctx_result = AesGcmContext::create(zero_key);
    ASSERT_FALSE(ctx_result.has_value());
    EXPECT_EQ(ctx_result.error(), CryptoError::EncryptionFailed);
}

// Move semantics: a moved-from context's resources should be properly
// transferred to the destination, and the destination remains usable.
TEST_F(AesGcmTest, ContextMoveConstructible) {
    const auto key = random_key();
    auto src = AesGcmContext::create(key).value();
    auto dst = std::move(src);

    const auto plaintext = as_bytes("after move");
    auto enc = dst.encrypt(plaintext);
    ASSERT_TRUE(enc.has_value());
    auto dec = dst.decrypt(enc->ciphertext, enc->iv);
    ASSERT_TRUE(dec.has_value());
    EXPECT_TRUE(std::equal(plaintext.begin(), plaintext.end(),
                           dec->plaintext.begin()));
}

TEST_F(AesGcmTest, ContextMoveAssignable) {
    const auto key1 = random_key();
    const auto key2 = random_key();
    auto a = AesGcmContext::create(key1).value();
    auto b = AesGcmContext::create(key2).value();
    a = std::move(b);  // 'a' now uses key2

    auto enc = a.encrypt(as_bytes("ping"));
    ASSERT_TRUE(enc.has_value());
    // Free-function decrypt with key2 should succeed; with key1 should fail.
    EXPECT_TRUE(decrypt(enc->ciphertext, key2, enc->iv).has_value());
    EXPECT_FALSE(decrypt(enc->ciphertext, key1, enc->iv).has_value());
}

// --- Coverage: errors.hpp lines 21-29 (to_string all CryptoError enum values) ---
TEST_F(AesGcmTest, CryptoErrorToStringAllValues) {
    EXPECT_EQ(to_string(CryptoError::InvalidKeySize), "Invalid key size");
    EXPECT_EQ(to_string(CryptoError::InvalidIVSize), "Invalid IV size");
    EXPECT_EQ(to_string(CryptoError::EncryptionFailed), "Encryption failed");
    EXPECT_EQ(to_string(CryptoError::DecryptionFailed), "Decryption failed");
    EXPECT_EQ(to_string(CryptoError::AuthenticationFailed), "Authentication failed");
    EXPECT_EQ(to_string(CryptoError::InvalidSignature), "Invalid signature");
    EXPECT_EQ(to_string(CryptoError::KeyDerivationFailed), "Key derivation failed");
}

} // namespace
} // namespace euxis::crypto
