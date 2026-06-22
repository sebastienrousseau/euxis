/// @file
/// @brief Canonical SCA (Software Composition Analysis) manifest types.
///
/// Each ecosystem ships its own lockfile format (Cargo.lock,
/// package-lock.json, Pipfile.lock, go.sum, …). This header defines
/// the unified intermediate representation so downstream code — the
/// SBOM emitter, the OSV.dev matcher, the reachability pruner — has a
/// single shape to consume.
///
/// Each parser in this library converts its native format into a
/// `ParsedManifest` containing a vector of `ManifestEntry` and an
/// optional dependency graph. The directory scanner in scanner.hpp
/// builds a euxis::sbom::SbomDocument by walking a tree, dispatching
/// to the right parser per filename, and merging the results.
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "euxis/sbom/types.hpp"

namespace euxis::sca {

/// Recognised package ecosystems. Maps 1:1 to purl `type` tokens.
enum class Ecosystem {
    Cargo,    ///< Rust crates.io
    Npm,      ///< Node.js, package-lock.json v3
    Pypi,     ///< Python, Pipfile.lock
    Golang,   ///< Go, go.sum + go.mod
    Maven,    ///< Java/Kotlin, pom.xml / build.gradle (deferred)
    Gem,      ///< Ruby, Gemfile.lock (deferred)
    Composer, ///< PHP, composer.lock (deferred)
};

/// Stringified ecosystem name. Used for diagnostics.
[[nodiscard]] auto ecosystem_str(Ecosystem e) noexcept -> const char*;

/// Single entry in a lockfile.
struct ManifestEntry {
    /// Package name as it appears in the lockfile.
    std::string name;

    /// Resolved version string (verbatim, even if it's "==1.0.0").
    std::string version;

    /// Optional purl-style namespace (npm @scope, Maven groupId, Go
    /// module path prefix).
    std::string ns;

    /// Cryptographic hash of the resolved artefact when the lockfile
    /// carries one. npm has integrity, Cargo has checksum, go.sum
    /// has h1:, Pipfile.lock has sha256:.
    std::optional<euxis::sbom::Hash> hash;

    /// Whether the package is a direct dependency (declared in
    /// Cargo.toml, package.json, Pipfile, go.mod) versus
    /// transitively pulled in. Conservative default: false so a
    /// downstream user-facing summary doesn't overclaim.
    bool is_direct{false};

    /// Whether the package is dev-only (Pipfile.lock develop,
    /// package-lock.json devDependencies). Defaults to false.
    bool is_dev{false};

    /// Source / registry URL when present (e.g. crates.io,
    /// registry.npmjs.org). Empty for vendored or git-source
    /// dependencies.
    std::string source;

    /// Direct dependencies of this entry by `name`. The directory
    /// scanner cross-references these to build the SbomDocument
    /// dependency graph.
    std::vector<std::string> depends_on;
};

/// One parsed lockfile. The directory scanner emits one of these per
/// file it dispatches to.
struct ParsedManifest {
    Ecosystem ecosystem{};  // always overwritten by the parser; the brace-init
                            // silences cppcoreguidelines-pro-type-member-init.
    std::filesystem::path source_file;
    std::vector<ManifestEntry> entries;

    /// Best-effort root component (e.g. the workspace root in
    /// Cargo.lock, the root package in package-lock.json).
    std::optional<ManifestEntry> root;
};

/// Parse error with file + line context for diagnostics.
struct ParseError {
    std::string message;
    std::filesystem::path file;
    int line{0};
};

/// Convert a parsed entry to a euxis::sbom::Component.
[[nodiscard]] auto to_component(const ManifestEntry& entry,
                                Ecosystem ecosystem) -> euxis::sbom::Component;

/// Construct a purl string for the given entry/ecosystem.
[[nodiscard]] auto to_purl(const ManifestEntry& entry,
                           Ecosystem ecosystem) -> std::string;

} // namespace euxis::sca
