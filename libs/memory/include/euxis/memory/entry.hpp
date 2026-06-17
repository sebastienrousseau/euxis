/// @file
/// @brief Memory entry
#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "tier.hpp"

namespace euxis::memory {

struct EncryptedMemoryEntry {
    std::string entry_id;       // UUID hex
    MemoryTier tier{};
    std::string ciphertext_b64; // Base64-encoded ciphertext
    std::string created_at;     // UTC ISO-8601
    std::string agent_did;

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> EncryptedMemoryEntry;
};

} // namespace euxis::memory
