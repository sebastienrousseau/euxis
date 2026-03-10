#pragma once

#include <string>

namespace euxis::crypto {

/**
 * @brief Error codes for the crypto module.
 */
enum class CryptoError {
    InvalidKeySize,      ///< Key buffer was not exactly 32 bytes.
    InvalidIVSize,       ///< IV buffer was not exactly 12 bytes.
    EncryptionFailed,    ///< AES-GCM encryption routine failed.
    DecryptionFailed,    ///< Authentication tag mismatch or corrupt ciphertext.
    AuthenticationFailed, ///< MAC verification failed during decryption.
    InvalidSignature,    ///< Signature verification failed.
    KeyGenerationFailed, ///< Secure random key generation failed.
    KeyDerivationFailed, ///< Argon2id or BLAKE2b derivation failed.
    InvalidInput,        ///< Buffer size mismatch or null data.
};

/**
 * @brief Convert a CryptoError enum to a human-readable string.
 * @param error The error code.
 * @return std::string The descriptive string.
 */
[[nodiscard]] auto to_string(CryptoError error) -> std::string;

} // namespace euxis::crypto
