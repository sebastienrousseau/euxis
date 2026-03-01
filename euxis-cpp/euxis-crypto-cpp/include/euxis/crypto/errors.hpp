#pragma once

#include <string>
#include <string_view>

namespace euxis::crypto {

enum class CryptoError {
    InvalidKeySize,
    InvalidIVSize,
    EncryptionFailed,
    DecryptionFailed,
    AuthenticationFailed,
    InvalidSignature,
    KeyDerivationFailed
};

/// Human-readable description of a CryptoError value.
[[nodiscard]] constexpr std::string_view to_string(CryptoError e) noexcept {
    switch (e) {
    case CryptoError::InvalidKeySize:       return "Invalid key size";
    case CryptoError::InvalidIVSize:        return "Invalid IV size";
    case CryptoError::EncryptionFailed:     return "Encryption failed";
    case CryptoError::DecryptionFailed:     return "Decryption failed";
    case CryptoError::AuthenticationFailed: return "Authentication failed";
    case CryptoError::InvalidSignature:     return "Invalid signature";
    case CryptoError::KeyDerivationFailed:  return "Key derivation failed";
    }
    return "Unknown crypto error";
}

} // namespace euxis::crypto
