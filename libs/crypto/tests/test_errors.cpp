#include "euxis/crypto/errors.hpp"

#include <gtest/gtest.h>

namespace euxis::crypto {
namespace {

TEST(CryptoErrorTest, AllEnumValuesHaveStrings) {
    // Verify every CryptoError enum maps to a non-empty, non-"Unknown" string
    EXPECT_EQ(to_string(CryptoError::InvalidKeySize), "Invalid key size");
    EXPECT_EQ(to_string(CryptoError::InvalidIVSize), "Invalid IV size");
    EXPECT_EQ(to_string(CryptoError::EncryptionFailed), "Encryption failed");
    EXPECT_EQ(to_string(CryptoError::DecryptionFailed), "Decryption failed");
    EXPECT_EQ(to_string(CryptoError::AuthenticationFailed), "Authentication failed");
    EXPECT_EQ(to_string(CryptoError::InvalidSignature), "Invalid signature");
    EXPECT_EQ(to_string(CryptoError::KeyGenerationFailed), "Key generation failed");
    EXPECT_EQ(to_string(CryptoError::KeyDerivationFailed), "Key derivation failed");
    EXPECT_EQ(to_string(CryptoError::InvalidInput), "Invalid input");
}

TEST(CryptoErrorTest, UnknownValueReturnsUnknown) {
    // Out-of-range enum value should return "Unknown error"
    auto bad = static_cast<CryptoError>(999);
    EXPECT_EQ(to_string(bad), "Unknown error");
}

} // namespace
} // namespace euxis::crypto
