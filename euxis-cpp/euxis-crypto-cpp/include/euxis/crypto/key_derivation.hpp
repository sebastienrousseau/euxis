#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "errors.hpp"

namespace euxis::crypto {

/// Result of a key derivation operation, containing both the derived key and
/// the salt that was used (so the caller can persist it for later re-derivation).
struct DerivedKey {
    std::vector<std::byte> key;
    std::vector<std::byte> salt;
};

/// Derive a key from `password` using Argon2id (via libsodium's crypto_pwhash).
///
/// @param password  Raw password bytes.
/// @param salt      Salt bytes. Must be exactly crypto_pwhash_SALTBYTES (16) bytes.
/// @param iterations  Argon2id opslimit. Defaults to 100'000 (mapped to
///                    crypto_pwhash_OPSLIMIT_SENSITIVE when >= 100'000, otherwise
///                    used directly clamped to libsodium's minimum).
/// @param key_size  Desired key length in bytes. Defaults to 32.
[[nodiscard]] auto derive_key(std::span<const std::byte> password,
                              std::span<const std::byte> salt,
                              uint32_t iterations = 100'000,
                              size_t key_size = 32)
    -> std::expected<DerivedKey, CryptoError>;

/// Generate a cryptographically-secure random key of the given size.
/// Uses libsodium's `randombytes_buf`.
[[nodiscard]] auto generate_key(size_t size = 32) -> std::vector<std::byte>;

} // namespace euxis::crypto
