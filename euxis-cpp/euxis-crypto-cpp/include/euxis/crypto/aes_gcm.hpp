#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "errors.hpp"
#include "types.hpp"

namespace euxis::crypto {

/// Encrypt `data` with AES-256-GCM using the given 32-byte key.
/// A random 12-byte IV (nonce) is generated internally via `randombytes_buf`.
/// The returned ciphertext includes the 16-byte auth tag appended by libsodium.
[[nodiscard]] auto encrypt(std::span<const std::byte> data,
                           std::span<const std::byte, 32> key)
    -> std::expected<EncryptionResult, CryptoError>;

/// Decrypt AES-256-GCM ciphertext (with appended auth tag) using the given key
/// and 12-byte IV that was used during encryption.
[[nodiscard]] auto decrypt(std::span<const std::byte> ciphertext,
                           std::span<const std::byte, 32> key,
                           std::span<const std::byte, 12> iv)
    -> std::expected<DecryptionResult, CryptoError>;

/// Encrypt `data` with AES-256-GCM using the given key and additional
/// authenticated data (AAD). The AAD is authenticated but NOT encrypted.
[[nodiscard]] auto encrypt_aad(std::span<const std::byte> data,
                               std::span<const std::byte, 32> key,
                               std::span<const std::byte> aad)
    -> std::expected<EncryptionResult, CryptoError>;

/// Decrypt AES-256-GCM ciphertext that was encrypted with AAD.
/// The same AAD must be provided for authentication to succeed.
[[nodiscard]] auto decrypt_aad(std::span<const std::byte> ciphertext,
                               std::span<const std::byte, 32> key,
                               std::span<const std::byte, 12> iv,
                               std::span<const std::byte> aad)
    -> std::expected<DecryptionResult, CryptoError>;

} // namespace euxis::crypto
