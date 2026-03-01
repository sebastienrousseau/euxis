#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace euxis::crypto {

/// Result of an AES-256-GCM encryption operation.
/// The ciphertext includes the 16-byte authentication tag appended by libsodium.
struct EncryptionResult {
    std::vector<std::byte> ciphertext;
    std::array<std::byte, 12> iv;
    std::optional<std::vector<std::byte>> salt;
    std::string algorithm = "AES-256-GCM";

    /// Serialise the entire result (iv + ciphertext) to a Base64 string
    /// using libsodium's sodium_bin2base64 with VARIANT_ORIGINAL.
    [[nodiscard]] std::string to_base64() const;
};

/// Result of a successful decryption operation.
struct DecryptionResult {
    std::vector<std::byte> plaintext;
    std::string algorithm;

    /// Interpret the plaintext bytes as a UTF-8 string.
    [[nodiscard]] std::string to_string() const;
};

} // namespace euxis::crypto
