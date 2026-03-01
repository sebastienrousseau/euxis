#include "euxis/memory/entry.hpp"

#include <stdexcept>

namespace euxis::memory {

namespace {

/// Map a tier label string back to the enum.
[[nodiscard]] auto tier_from_label(std::string_view label) -> MemoryTier {
    if (label == "hot")         return MemoryTier::Hot;
    if (label == "relevant")    return MemoryTier::Relevant;
    if (label == "cross_agent") return MemoryTier::CrossAgent;
    throw std::invalid_argument(
        std::string("Unknown memory tier label: ") + std::string(label));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// to_json
// ---------------------------------------------------------------------------
nlohmann::json EncryptedMemoryEntry::to_json() const {
    return nlohmann::json{
        {"entry_id",   entry_id},
        {"tier",       std::string(tier_label(tier))},
        {"ciphertext", ciphertext_b64},
        {"created_at", created_at},
        {"agent_did",  agent_did}
    };
}

// ---------------------------------------------------------------------------
// from_json
// ---------------------------------------------------------------------------
auto EncryptedMemoryEntry::from_json(const nlohmann::json& j)
    -> EncryptedMemoryEntry {
    EncryptedMemoryEntry e;
    e.entry_id       = j.at("entry_id").get<std::string>();
    e.tier           = tier_from_label(j.at("tier").get<std::string>());
    e.ciphertext_b64 = j.at("ciphertext").get<std::string>();
    e.created_at     = j.at("created_at").get<std::string>();
    e.agent_did      = j.at("agent_did").get<std::string>();
    return e;
}

} // namespace euxis::memory
