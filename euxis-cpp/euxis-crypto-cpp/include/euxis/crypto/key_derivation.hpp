#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "errors.hpp"

namespace euxis::crypto {

/**
 * @brief Result of a key derivation operation.
 */
struct DerivedKey {
    std::vector<std::byte> key;  ///< The actual derived bytes.
    std::vector<std::byte> salt; ///< The salt used (for persistence).
};

/**
 * @brief Derive a key from a password or seed.
 * 
 * Supports two modes:
 * 1. Argon2id (iterations > 0): Slow, memory-hard hashing for passwords.
 * 2. BLAKE2b (iterations == 0): Fast FastPath for deterministic session keys.
 *
 * @param password Raw password or seed bytes.
 * @param salt Salt bytes. Argon2id requires exactly 16 bytes.
 * @param iterations Use 0 for high-performance BLAKE2b FastPath.
 * @param key_size Desired output length in bytes.
 * @return std::expected<DerivedKey, CryptoError> The derived key or error.
 */
[[nodiscard]] auto derive_key(std::span<const std::byte> password,
                              std::span<const std::byte> salt,
                              uint32_t iterations = 100'000,
                              size_t key_size = 32)
    -> std::expected<DerivedKey, CryptoError>;

/**
 * @brief Generate a cryptographically-secure random key.
 * @param size Desired key length.
 * @return std::vector<std::byte> Random bytes.
 */
[[nodiscard]] auto generate_key(size_t size = 32) -> std::vector<std::byte>;

} // namespace euxis::crypto
