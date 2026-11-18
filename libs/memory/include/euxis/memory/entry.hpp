/// @file
/// @brief Memory entry
#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "tier.hpp"

namespace euxis::memory {

/// AEAD written with every new entry (24-byte nonce prefix).
inline constexpr std::string_view kAlgXChaCha = "XChaCha20-Poly1305";
/// Entries without an "alg" field predate it and were sealed with
/// AES-256-GCM (12-byte IV prefix). Readable where the CPU has AES
/// instructions; EncryptedMemoryStore::migrate_legacy_entries() rewrites
/// them as XChaCha20-Poly1305.
inline constexpr std::string_view kAlgLegacyAesGcm = "AES-256-GCM";

struct EncryptedMemoryEntry {
    std::string entry_id;       // UUID hex
    MemoryTier tier{};
    std::string ciphertext_b64; // Base64-encoded ciphertext
    std::string created_at;     // UTC ISO-8601
    std::string agent_did;
    std::string alg{kAlgLegacyAesGcm}; // AEAD that sealed ciphertext_b64

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> EncryptedMemoryEntry;
};

} // namespace euxis::memory
