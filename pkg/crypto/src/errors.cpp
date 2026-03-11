#include "euxis/crypto/errors.hpp"

namespace euxis::crypto {

auto to_string(CryptoError error) -> std::string {
    switch (error) {
        case CryptoError::InvalidKeySize:      return "Invalid key size";
        case CryptoError::InvalidIVSize:       return "Invalid IV size";
        case CryptoError::EncryptionFailed:    return "Encryption failed";
        case CryptoError::DecryptionFailed:    return "Decryption failed";
        case CryptoError::AuthenticationFailed: return "Authentication failed";
        case CryptoError::InvalidSignature:    return "Invalid signature";
        case CryptoError::KeyGenerationFailed: return "Key generation failed";
        case CryptoError::KeyDerivationFailed: return "Key derivation failed";
        case CryptoError::InvalidInput:        return "Invalid input";
    }
    return "Unknown error";
}

} // namespace euxis::crypto
