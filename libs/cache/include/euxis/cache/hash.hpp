/// @file
/// @brief BLAKE2b content hashing for the scan cache.
///
/// libsodium ships `crypto_generichash` which is BLAKE2b; we use it
/// rather than rolling our own because:
///   - libsodium is already a build dep (libs/crypto links against it)
///   - the implementation is constant-time and side-channel-hardened
///   - BLAKE2b-256 is a fixed 32-byte digest, exactly what the cache
///     key shape expects
///
/// `Digest` is a hex-encoded BLAKE2b-256 (64 lowercase hex chars).
/// Cache keys, file content, ruleset payloads all use the same alg
/// and length so the table schema is uniform.
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace euxis::cache {

/// Output length in BYTES of the BLAKE2b digest used everywhere in
/// this library. 32 → 256 bits → 64 hex characters.
inline constexpr std::size_t kDigestBytes = 32;
inline constexpr std::size_t kDigestHexLen = kDigestBytes * 2;

/// Hash error.
struct HashError {
    std::string message;
    std::filesystem::path file;
};

/// Compute BLAKE2b-256 of an in-memory byte span and return its hex
/// digest. Will not fail for in-memory input; the signature returns
/// `std::string` directly.
[[nodiscard]] auto hash_bytes(std::span<const std::byte> data) -> std::string;

/// Convenience overload for string-like input.
[[nodiscard]] auto hash_string(std::string_view s) -> std::string;

/// Compute BLAKE2b-256 of a file's content, streaming so memory use
/// stays bounded regardless of file size. Returns the hex digest.
[[nodiscard]] auto hash_file(const std::filesystem::path& path)
    -> std::expected<std::string, HashError>;

/// True when `s` is a syntactically valid hex digest of the expected
/// length.
[[nodiscard]] auto is_valid_digest(std::string_view s) noexcept -> bool;

/// Stream-style incremental hasher. Useful when the caller wants to
/// combine multiple inputs (e.g. a ruleset across many YAML files)
/// into a single digest without concatenating in memory.
class Hasher {
public:
    Hasher();
    ~Hasher();
    Hasher(const Hasher&)            = delete;
    Hasher& operator=(const Hasher&) = delete;
    Hasher(Hasher&&) noexcept;
    Hasher& operator=(Hasher&&) noexcept;

    void update(std::span<const std::byte> data) noexcept;
    void update(std::string_view s) noexcept;

    /// Returns the hex digest. The hasher is consumed; subsequent
    /// `update()` calls have unspecified behaviour.
    [[nodiscard]] auto finalize() -> std::string;

private:
    struct State;
    State* state_;  // owned; explicit so libs/cache headers don't
                    // need to include sodium.h
};

} // namespace euxis::cache
