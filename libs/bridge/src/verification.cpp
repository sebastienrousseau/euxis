#include <euxis/bridge/verification.hpp>
#include <euxis/crypto/ed25519.hpp>

#include <cstring>
#include <fstream>
#include <vector>
#include <sodium.h>

namespace euxis::bridge {

auto verify_skill_signature(
    const std::filesystem::path& file_path,
    const std::filesystem::path& signature_path,
    const std::array<std::byte, 32>& public_key
) -> std::expected<VerificationResult, std::string> {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected("Cannot open file: " + file_path.string());
    }
    std::string file_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    std::vector<std::byte> file_bytes(file_str.size());
    std::memcpy(file_bytes.data(), file_str.data(), file_str.size());

    // Read the signature (64 bytes)
    std::ifstream sig_file(signature_path, std::ios::binary);
    if (!sig_file.is_open()) {
        return std::unexpected("Cannot open signature: " + signature_path.string());
    }
    std::string sig_str((std::istreambuf_iterator<char>(sig_file)),
                         std::istreambuf_iterator<char>());
    if (sig_str.size() != 64) {
        return std::unexpected("Invalid signature size: expected 64 bytes, got "
                               + std::to_string(sig_str.size()));
    }
    std::array<std::byte, 64> signature{};
    std::memcpy(signature.data(), sig_str.data(), 64);

    bool valid = euxis::crypto::verify(
        std::span<const std::byte, 32>{public_key},
        std::span<const std::byte>{file_bytes},
        std::span<const std::byte, 64>{signature}
    );

    VerificationResult result;
    result.valid = valid;
    result.signer = valid ? "verified" : "unknown";
    result.message = valid ? "Signature verified successfully"
                          : "Signature verification failed";
    return result;
}

auto load_public_key(const std::filesystem::path& key_path)
    -> std::expected<std::array<std::byte, 32>, std::string> {
    std::ifstream file(key_path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected("Cannot open key file: " + key_path.string());
    }
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    if (data.size() == 32) {
        // Raw binary key
        std::array<std::byte, 32> key{};
        std::memcpy(key.data(), data.data(), 32);
        return key;
    }

    // Try base64 decode
    std::vector<unsigned char> decoded(32);
    size_t decoded_len = 0;
    if (sodium_base642bin(decoded.data(), decoded.size(),
                          data.c_str(), data.size(),
                          nullptr, &decoded_len, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) == 0
        && decoded_len == 32) {
        std::array<std::byte, 32> key{};
        std::memcpy(key.data(), decoded.data(), 32);
        return key;
    }

    return std::unexpected("Invalid key format: expected 32 bytes raw or base64");
}

}  // namespace euxis::bridge
