#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "aes_gcm.hpp"
#include "errors.hpp"

namespace euxis::crypto {

/// XChaCha20-Poly1305 (IETF), libsodium's recommended AEAD. Pure software,
/// constant time, and available on every CPU, unlike AES-256-GCM, which
/// libsodium only offers with AES instructions. Its 192-bit nonce is safe
/// to draw at random for every message under one key.
inline constexpr std::string_view kXChaChaAlgorithm = "XChaCha20-Poly1305";
inline constexpr std::size_t kXChaChaNonceBytes = 24;
inline constexpr std::size_t kXChaChaTagBytes = 16;

/** @brief Ciphertext (with its 16-byte tag appended) and its nonce. */
struct SealedBox {
    std::vector<std::byte> ciphertext;
    std::array<std::byte, kXChaChaNonceBytes> nonce{};
};

/** @brief Encrypt with a fresh random nonce; aad is authenticated, not encrypted. */
[[nodiscard]] auto xchacha_encrypt(std::span<const std::byte> data,
                                   std::span<const std::byte, 32> key,
                                   std::span<const std::byte> aad = {})
    -> std::expected<SealedBox, CryptoError>;

/** @brief Decrypt and verify; AuthenticationFailed on any tampering. */
[[nodiscard]] auto xchacha_decrypt(std::span<const std::byte> ciphertext,
                                   std::span<const std::byte, 32> key,
                                   std::span<const std::byte, kXChaChaNonceBytes> nonce,
                                   std::span<const std::byte> aad = {})
    -> std::expected<DecryptionResult, CryptoError>;

} // namespace euxis::crypto
