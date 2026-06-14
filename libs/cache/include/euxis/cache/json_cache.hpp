/// @file
/// @brief Content-keyed cache for parsed nlohmann::json documents.
///
/// Issue #60 surfaced that `nlohmann::json::parse` of the 53 agent
/// registry JSON files is ~5.7 % of CLI startup CPU. Every short-
/// lived CLI invocation pays the full parse cost; the user-visible
/// effect is sluggish `euxis <subcommand>` start-up in CI shell
/// loops.
///
/// The remediation is this primitive — a content-keyed cache that
/// stores the **post-parse** JSON tree as msgpack on disk, keyed on
/// the file's BLAKE2b-256 content hash. On a cache hit the caller
/// avoids the parser entirely and goes straight to a fast
/// msgpack-from_bytes deserialise.
///
/// Storage layout: SQLite (same backing as the existing
/// `ScanCache`) so we don't introduce a new on-disk format. The
/// schema lives under a dedicated table so cache lifetimes are
/// independent of the scan cache's TTL.
///
/// The Week 17 batch ships the primitive only — the
/// `RegistryClient` swap-in lands in a follow-up that:
///   1. Wraps each `nlohmann::json::parse(f)` call in a
///      `cache.get_or_load(path, parse_fn)` helper.
///   2. Asserts a regression test against the existing 53-file
///      fixture (`docs/benchmarks/registry-parse.md`) — warm-cache
///      should be < 5 % of cold-cache wall time.
///   3. Drops issue #60 to the closed pile.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::cache {

struct JsonCacheError {
    std::string message;
};

struct JsonCacheStats {
    std::size_t hits{0};
    std::size_t misses{0};
    std::size_t bytes_stored{0};
    std::size_t entries{0};
};

/// Cache for parsed nlohmann::json documents. Keys are absolute
/// file paths; values are msgpack-encoded JSON trees plus the
/// source file's BLAKE2b-256 content hash. A cache hit verifies the
/// stored hash matches the current file content before returning,
/// so an out-of-band edit of the source file invalidates the entry
/// even if the path is unchanged.
class JsonCache {
public:
    /// Open (or create) the cache at `db_path`. Parent directories
    /// are created if missing. Returns a JsonCache on success or an
    /// error on SQLite / I/O failure.
    [[nodiscard]] static auto open(const std::filesystem::path& db_path)
        -> std::expected<JsonCache, JsonCacheError>;

    ~JsonCache();

    JsonCache(const JsonCache&)             = delete;
    JsonCache& operator=(const JsonCache&)  = delete;
    JsonCache(JsonCache&&) noexcept;
    JsonCache& operator=(JsonCache&&) noexcept;

    /// Look up the parsed JSON for `file`. Returns `std::nullopt`
    /// on cache miss (no entry, or hash mismatch from an edit).
    /// Errors surface I/O / SQLite / hash-computation failures.
    [[nodiscard]] auto get(const std::filesystem::path& file) const
        -> std::expected<std::optional<nlohmann::json>, JsonCacheError>;

    /// Insert (or replace) the entry for `file`. The caller has
    /// already parsed the file; this helper recomputes the content
    /// hash so the next `get()` can validate.
    auto put(const std::filesystem::path& file, const nlohmann::json& parsed)
        -> std::expected<void, JsonCacheError>;

    /// Convenience: fast path for the "load this file, parse it if
    /// not cached" flow. Calls `get`; on miss reads the file,
    /// applies `parser_fn` to produce a `nlohmann::json`, stores it,
    /// and returns the result. `parser_fn` defaults to a vanilla
    /// `nlohmann::json::parse` so callers that don't need custom
    /// parsing don't have to supply one.
    [[nodiscard]] auto get_or_load(const std::filesystem::path& file)
        -> std::expected<nlohmann::json, JsonCacheError>;

    /// Remove every cached entry.
    [[nodiscard]] auto purge() -> std::expected<std::size_t, JsonCacheError>;

    /// Aggregate statistics. Populated lazily; resets on `purge`.
    [[nodiscard]] auto stats() const noexcept -> JsonCacheStats;

    /// Path the cache database lives at.
    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path&;

private:
    JsonCache();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::cache
