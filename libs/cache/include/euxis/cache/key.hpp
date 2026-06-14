/// @file
/// @brief Composite cache key.
///
/// A scan result is cacheable iff:
///   - the file's content has not changed (BLAKE2b of its bytes), AND
///   - the ruleset the scanner consumed has not changed, AND
///   - the euxis binary version has not changed.
///
/// All three are folded into a single BLAKE2b digest that becomes
/// the SQLite primary key. The constituent fields are also stored
/// so `euxis cache list` can render human-readable rows and the
/// LRU/TTL housekeeping can reason about staleness.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace euxis::cache {

/// Inputs to `compose_key()`. All fields are required.
struct KeyInputs {
    std::filesystem::path file;     ///< absolute path; relative path is rejected
    std::string content_hash;       ///< BLAKE2b-256 hex (64 chars)
    std::string ruleset_hash;       ///< BLAKE2b-256 hex (64 chars)
    std::string_view tool_version;  ///< euxis CLI version string, e.g. "0.1.0"
};

/// Hex-encoded BLAKE2b-256 of all four inputs concatenated with NUL
/// separators. Suitable as a SQLite primary key — short, fixed
/// length, no path-traversal risk.
[[nodiscard]] auto compose_key(const KeyInputs& inputs) -> std::string;

/// The other half of the schema: a single cached scan result.
struct CacheEntry {
    std::string findings_json;      ///< canonical JSON form
    std::int64_t created_at_unix{0};
    std::int64_t size_bytes{0};
};

} // namespace euxis::cache
