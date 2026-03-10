/// @file
/// @brief Cryptographic verification for bridged skills.
#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <string>
#include <span>

namespace euxis::bridge {

/// @brief Result of a signature verification operation.
struct VerificationResult {
    bool valid{false};
    std::string signer;
    std::string message;
};

/// @brief Verify the Ed25519 signature of a skill file.
auto verify_skill_signature(const std::filesystem::path& file_path,
                            const std::filesystem::path& signature_path,
                            const std::array<std::byte, 32>& public_key)
    -> std::expected<VerificationResult, std::string>;

/// @brief Load a raw 32-byte public key from a file.
auto load_public_key(const std::filesystem::path& key_path)
    -> std::expected<std::array<std::byte, 32>, std::string>;

} // namespace euxis::bridge
