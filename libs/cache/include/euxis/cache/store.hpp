/// @file
/// @brief SQLite-backed incremental scan cache.
///
/// The cache exists so re-running a scan on an unchanged repository
/// completes in well under a second, unblocking the "45-second
/// triage on a 100K-LOC repo" SLO from ~/Drop/euxis-ip.md.
///
/// Schema:
///
///   CREATE TABLE scan_cache (
///       cache_key     TEXT    PRIMARY KEY,    -- compose_key() output
///       file_path     TEXT    NOT NULL,
///       content_hash  TEXT    NOT NULL,
///       ruleset_hash  TEXT    NOT NULL,
///       tool_version  TEXT    NOT NULL,
///       findings_json BLOB,
///       created_at    INTEGER NOT NULL,       -- unix seconds
///       size_bytes    INTEGER NOT NULL
///   );
///   CREATE INDEX idx_created_at ON scan_cache(created_at);
///
/// Concurrency: the SQLite handle uses `PRAGMA journal_mode=WAL` so
/// multiple readers can coexist with at most one writer. We do not
/// try to share a single handle across threads inside the same
/// process; callers should construct one `ScanCache` per worker.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "euxis/cache/key.hpp"

namespace euxis::cache {

struct StoreError {
    std::string message;
    int sqlite_code{0};
};

struct CacheStats {
    std::int64_t entry_count{0};
    std::int64_t total_size_bytes{0};
    std::int64_t oldest_entry_unix{0};
    std::int64_t newest_entry_unix{0};
};

/// Persistent scan-result cache. PIMPL wraps the sqlite3* handle so
/// the header stays SQLite-free.
class ScanCache {
public:
    /// Open (or create) the cache database at `path`. The parent
    /// directory is created if missing. Subsequent operations on a
    /// failed-open ScanCache return errors deterministically.
    [[nodiscard]] static auto open(const std::filesystem::path& path)
        -> std::expected<ScanCache, StoreError>;

    ~ScanCache();
    ScanCache(const ScanCache&)            = delete;
    ScanCache& operator=(const ScanCache&) = delete;
    ScanCache(ScanCache&&) noexcept;
    ScanCache& operator=(ScanCache&&) noexcept;

    /// Look up an entry by composite key. Returns `std::nullopt`
    /// when the key is absent (cache miss).
    [[nodiscard]] auto get(const std::string& cache_key) const
        -> std::expected<std::optional<CacheEntry>, StoreError>;

    /// Insert or replace an entry. The `inputs` are persisted as
    /// individual columns so `list()` can render them later.
    auto put(const KeyInputs& inputs, const CacheEntry& entry)
        -> std::expected<void, StoreError>;

    /// Remove every row. Returns the number deleted.
    [[nodiscard]] auto purge() -> std::expected<std::int64_t, StoreError>;

    /// Remove rows whose `created_at` is older than `now_unix - ttl`.
    [[nodiscard]] auto purge_older_than(std::int64_t ttl_seconds,
                                         std::int64_t now_unix)
        -> std::expected<std::int64_t, StoreError>;

    /// Aggregate statistics. Useful for `euxis cache stats`.
    [[nodiscard]] auto stats() const
        -> std::expected<CacheStats, StoreError>;

    /// Path the cache database lives at.
    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path&;

private:
    ScanCache();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::cache
