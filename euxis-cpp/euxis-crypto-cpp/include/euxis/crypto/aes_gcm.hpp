#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "errors.hpp"

namespace euxis::crypto {

/** @brief Result of a successful encryption. */
struct EncryptionResult {
    std::vector<std::byte> ciphertext;
    std::array<std::byte, 12> iv;
    std::string algorithm;

    /** @brief Encodes IV + Ciphertext as Base64. */
    [[nodiscard]] std::string to_base64() const;
};

/** @brief Result of a successful decryption. */
struct DecryptionResult {
    std::vector<std::byte> plaintext;
    std::string algorithm;

    /** @brief Convienence helper to get plaintext as UTF-8 string. */
    [[nodiscard]] std::string to_string() const;
};

/** @brief Encrypt data using AES-256-GCM. */
[[nodiscard]] auto encrypt(std::span<const std::byte> data,
                           std::span<const std::byte, 32> key)
    -> std::expected<EncryptionResult, CryptoError>;

/** @brief Decrypt data using AES-256-GCM. */
[[nodiscard]] auto decrypt(std::span<const std::byte> ciphertext,
                           std::span<const std::byte, 32> key,
                           std::span<const std::byte, 12> iv)
    -> std::expected<DecryptionResult, CryptoError>;

/** @brief Authenticated encryption with Additional Authenticated Data (AAD). */
[[nodiscard]] auto encrypt_aad(std::span<const std::byte> data,
                               std::span<const std::byte, 32> key,
                               std::span<const std::byte> aad)
    -> std::expected<EncryptionResult, CryptoError>;

/** @brief Authenticated decryption with Additional Authenticated Data (AAD). */
[[nodiscard]] auto decrypt_aad(std::span<const std::byte> ciphertext,
                               std::span<const std::byte, 32> key,
                               std::span<const std::byte, 12> iv,
                               std::span<const std::byte> aad)
    -> std::expected<DecryptionResult, CryptoError>;

} // namespace euxis::crypto
