#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>

namespace euxis::bridge {

struct VerificationResult {
    bool valid;
    std::string signer;
    std::string message;
};

/// Verify an Ed25519 detached signature for a file
[[nodiscard]] auto verify_skill_signature(
    const std::filesystem::path& file_path,
    const std::filesystem::path& signature_path,
    const std::array<std::byte, 32>& public_key
) -> std::expected<VerificationResult, std::string>;

/// Load a public key from a file (32 bytes, raw or base64)
[[nodiscard]] auto load_public_key(const std::filesystem::path& key_path)
    -> std::expected<std::array<std::byte, 32>, std::string>;

}  // namespace euxis::bridge
