/// @file
/// @brief Memory store
#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "entry.hpp"
#include "tier.hpp"

namespace euxis::memory {

class EncryptedMemoryStore {
public:
    EncryptedMemoryStore(
        std::filesystem::path store_dir,
        std::span<const std::byte, 32> master_key,
        std::string agent_did
    );

    [[nodiscard]] auto store(std::string_view content, MemoryTier tier = MemoryTier::Hot)
        -> std::expected<EncryptedMemoryEntry, std::string>;

    [[nodiscard]] auto retrieve(std::string_view entry_id)
        -> std::expected<std::string, std::string>;

    [[nodiscard]] auto retrieve_tier(MemoryTier tier, size_t limit = 20)
        -> std::vector<std::string>;

    void destroy_agent_keys();

    [[nodiscard]] auto export_tier_encrypted(MemoryTier tier)
        -> std::vector<EncryptedMemoryEntry>;
    /// Re-encrypt every legacy AES-256-GCM entry as XChaCha20-Poly1305,
    /// keeping its id, tier and timestamp. The file is rewritten through a
    /// temporary file and an atomic rename; lines it cannot parse are kept
    /// verbatim. All or nothing: if any legacy entry fails to decrypt
    /// (wrong key, or a CPU without AES instructions) nothing is written.
    /// Returns the number of entries migrated.
    [[nodiscard]] auto migrate_legacy_entries()
        -> std::expected<std::size_t, std::string>;

private:
    std::array<std::byte, 32> agent_key_;
    std::filesystem::path store_path_;
    std::string agent_did_;

    // In-memory cache: populated on first access, invalidated on store()
    mutable std::vector<EncryptedMemoryEntry> cache_;
    mutable std::unordered_map<std::string, size_t> cache_index_; // entry_id -> vector index
    mutable bool cache_valid_{false};

    void derive_agent_key(std::span<const std::byte, 32> master_key);
    auto entries() const -> const std::vector<EncryptedMemoryEntry>&;
    void invalidate_cache();
    auto load_entries_from_disk() const -> std::vector<EncryptedMemoryEntry>;
    void save_entry(const EncryptedMemoryEntry& entry);
};

} // namespace euxis::memory
