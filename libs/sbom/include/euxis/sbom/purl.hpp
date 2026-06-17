/// @file
/// @brief Package URL (purl) builder and parser.
///
/// Implements the purl spec maintained at
/// https://github.com/package-url/purl-spec. Currently aligned with
/// the v0.3 (May 2024) revision used by CycloneDX 1.6, SPDX 3.0.1,
/// OSV.dev, and GitHub Advisory Database.
///
/// Layout: scheme:type/namespace/name@version?qualifiers#subpath
///
///   pkg:npm/lodash@4.17.21
///   pkg:cargo/serde@1.0.197
///   pkg:pypi/django@5.0.4
///   pkg:golang/k8s.io/client-go@v0.32.1
///   pkg:maven/org.apache.commons/commons-lang3@3.14.0
///
/// Only the type, name, and version are required; namespace and
/// subpath are optional; qualifiers are an ordered key/value map.
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace euxis::sbom {

/// Canonical package type. Free-form strings are also accepted by
/// `Purl::build()` for ecosystems not in this list.
enum class PurlType {
    Npm,        ///< Node Package Manager
    Pypi,       ///< Python Package Index
    Cargo,      ///< Rust crates.io
    Golang,     ///< Go modules
    Maven,      ///< Maven Central
    Gem,        ///< RubyGems
    Nuget,      ///< NuGet
    Composer,   ///< PHP Composer / Packagist
    Hex,        ///< Erlang/Elixir Hex.pm
    Conan,      ///< C/C++ Conan
    Conda,      ///< Conda
    Github,     ///< GitHub source
    Generic,    ///< Generic / vendored
};

/// Convert a PurlType to its canonical purl `type` token.
[[nodiscard]] auto purl_type_str(PurlType t) noexcept -> const char*;

/// Parsed/built purl. Fields are stored decoded (the encode/decode
/// round-trip is handled by `to_string()` and `Purl::parse()`).
struct Purl {
    std::string type;                                       ///< e.g. "npm", "cargo"
    std::string ns;                                         ///< optional namespace; "@scope" for npm
    std::string name;                                       ///< required
    std::string version;                                    ///< optional but recommended
    std::unordered_map<std::string, std::string> qualifiers; ///< optional
    std::string subpath;                                    ///< optional

    /// Serialise to canonical purl string with spec-compliant
    /// percent-encoding for the segments that require it.
    [[nodiscard]] auto to_string() const -> std::string;

    /// Build from a typed PurlType. Convenience over the free-form
    /// `type` member.
    [[nodiscard]] static auto build(PurlType type,
                                    const std::string& name,
                                    const std::string& version,
                                    const std::string& ns = "") -> Purl;
};

/// Parse error.
struct PurlError {
    std::string message;
    std::size_t position{0};
};

/// Parse a purl string into a Purl. Returns `PurlError` on
/// malformed input.
[[nodiscard]] auto parse_purl(const std::string& s)
    -> std::expected<Purl, PurlError>;

/// Percent-encode for purl segments per the spec
/// (https://github.com/package-url/purl-spec/blob/main/PURL-SPECIFICATION.rst).
/// Visible for testing; production callers should use `Purl::to_string()`.
[[nodiscard]] auto percent_encode_segment(const std::string& s) -> std::string;

/// Percent-decode a purl segment.
[[nodiscard]] auto percent_decode_segment(const std::string& s) -> std::string;

} // namespace euxis::sbom
