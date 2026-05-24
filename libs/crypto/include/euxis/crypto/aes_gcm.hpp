#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "errors.hpp"

namespace euxis::crypto {

/** @brief Result of a successful encryption. */
struct EncryptionResult {
    std::vector<std::byte> ciphertext;
    std::array<std::byte, 12> iv{};  // default-init to zeros; populated by encrypt()
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

// ---------------------------------------------------------------------------
// AesGcmContext: caches the AES key schedule across many encrypt/decrypt
// operations. Profile (2026-05-24, Zen 5): the simple API above re-runs
// `aes256gcm_beforenm` (key schedule) on every call, accounting for ~15.6%
// of CPU on `crypto_throughput`. An isolated micro-bench using a cached
// state measured +30% throughput (1.29 → 1.68 M ops/s).
//
// Use the free functions for one-off encryption; use this context when many
// messages share one key (session encryption, memory store, channel adapters).
//
// Move-only: libsodium's state holds expanded round keys; copying would alias
// security-sensitive memory.
// ---------------------------------------------------------------------------
class AesGcmContext {
public:
    /** @brief Construct a context with a precomputed key schedule.
     *  @return AesGcmContext on success; CryptoError::EncryptionFailed if
     *  the key is all zeros (libsodium safety check). */
    [[nodiscard]] static auto create(std::span<const std::byte, 32> key)
        -> std::expected<AesGcmContext, CryptoError>;

    AesGcmContext(const AesGcmContext&) = delete;
    AesGcmContext& operator=(const AesGcmContext&) = delete;
    AesGcmContext(AesGcmContext&&) noexcept;
    AesGcmContext& operator=(AesGcmContext&&) noexcept;
    ~AesGcmContext();

    /** @brief Encrypt without AAD using the cached key schedule. */
    [[nodiscard]] auto encrypt(std::span<const std::byte> data) const
        -> std::expected<EncryptionResult, CryptoError>;

    /** @brief Encrypt with AAD using the cached key schedule. */
    [[nodiscard]] auto encrypt_aad(std::span<const std::byte> data,
                                   std::span<const std::byte> aad) const
        -> std::expected<EncryptionResult, CryptoError>;

    /** @brief Decrypt without AAD using the cached key schedule. */
    [[nodiscard]] auto decrypt(std::span<const std::byte> ciphertext,
                               std::span<const std::byte, 12> iv) const
        -> std::expected<DecryptionResult, CryptoError>;

    /** @brief Decrypt with AAD using the cached key schedule. */
    [[nodiscard]] auto decrypt_aad(std::span<const std::byte> ciphertext,
                                   std::span<const std::byte, 12> iv,
                                   std::span<const std::byte> aad) const
        -> std::expected<DecryptionResult, CryptoError>;

private:
    // Pimpl keeps <sodium.h> out of the public header (consumers' compile time).
    struct State;
    std::unique_ptr<State> state_;

    explicit AesGcmContext(std::unique_ptr<State> state) noexcept;
};

} // namespace euxis::crypto
