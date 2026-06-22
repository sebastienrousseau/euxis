/// @file
/// @brief Slopsquatting / hallucinated-package guard.
///
/// "Slopsquatting" is the highest-growth 2026 supply-chain attack
/// class — an attacker registers a package name that LLMs are known
/// to hallucinate, then ships malicious code when a developer
/// copy-pastes the import without checking. Spracklen et al.
/// (USENIX Security 2025) showed that across 16 code-generating LLMs
/// 19.7% of import suggestions name packages that do not exist;
/// the public dataset enumerates 205,474 unique hallucinated names.
///
/// This library ships with a curated seed corpus and exposes a
/// loader for the full Spracklen dataset. The check is purely
/// deterministic — no LLM is required at scan time — and runs
/// before purl construction so the SCA pipeline can refuse
/// resolution against compromised names.
///
/// Reference:
///   Spracklen et al., "We Have a Package for You! Comprehensive
///   Analysis of Package Hallucinations by Code-Generating LLMs"
///   USENIX Security 2025.
///   https://www.usenix.org/conference/usenixsecurity25/presentation/spracklen
#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "euxis/sbom/purl.hpp"  // for euxis::sbom::PurlType

namespace euxis::slopsquatting {

/// A single match between a project's manifest entry and a name in
/// the hallucination corpus.
struct Match {
    /// The exact name in the manifest that matched the corpus.
    std::string package_name;

    /// The ecosystem the name was looked up in. Slopsquatting is
    /// ecosystem-scoped: `requests` is fine for pypi but a known
    /// hallucination for cargo.
    euxis::sbom::PurlType ecosystem{euxis::sbom::PurlType::Generic};

    /// Optional: the legitimate name the corpus believes the
    /// developer meant. Empty when the match is an outright
    /// hallucination with no nearby legitimate target.
    std::string nearest_legitimate;

    /// Path to the manifest file where the match was found.
    std::filesystem::path source_file;
};

/// Errors from loading the corpus.
struct CorpusError {
    std::string message;
    std::filesystem::path file;
};

/// The guard. Constructed empty; populated by `load_seed()` and/or
/// `load_corpus_file()`. Thread-safe for concurrent reads after
/// construction is complete.
class Guard {
public:
    Guard() = default;

    /// Number of names in the corpus across all ecosystems.
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /// Returns true when `name` is in the corpus for `ecosystem`.
    /// `name` is matched case-sensitively for npm/cargo/go (the
    /// registries are case-sensitive); pypi normalises to lower-
    /// case + underscore-as-hyphen per PEP 503 §normalised name.
    [[nodiscard]] auto contains(std::string_view name,
                                euxis::sbom::PurlType ecosystem) const -> bool;

    /// Bulk add entries for a single ecosystem.
    void add(euxis::sbom::PurlType ecosystem, std::vector<std::string> names);

    /// Add a single (name, nearest legitimate) pair. When `nearest`
    /// is non-empty the value is surfaced in the resulting Match.
    void add_with_nearest(euxis::sbom::PurlType ecosystem,
                          std::string name,
                          std::string nearest);

    /// Load the curated seed corpus. This is the corpus embedded
    /// with the binary; safe to call exactly once at startup.
    /// Returns the number of entries added.
    auto load_seed() -> std::size_t;

    /// Load a Spracklen-format corpus file. The file is plain text
    /// with one entry per line, optionally prefixed with the
    /// ecosystem and an `=` separator for the nearest legitimate
    /// name:
    ///
    ///   # comment
    ///   npm: package-name
    ///   pypi: another-name = legit-name
    ///
    /// Lines without a `:` prefix are added to all ecosystems.
    [[nodiscard]] auto load_corpus_file(const std::filesystem::path& path)
        -> std::expected<std::size_t, CorpusError>;

private:
    struct EcosystemBucket {
        std::unordered_set<std::string> names;
        std::unordered_set<std::string> normalised_names;  // pypi only
        std::vector<std::pair<std::string, std::string>> nearest;
    };

    /// PEP 503 §normalised name: lowercase + `_` → `-`.
    [[nodiscard]] static auto pypi_normalise(std::string_view s) -> std::string;

    [[nodiscard]] auto bucket_for(euxis::sbom::PurlType e) -> EcosystemBucket&;
    [[nodiscard]] auto bucket_for(euxis::sbom::PurlType e) const -> const EcosystemBucket*;

    EcosystemBucket npm_;
    EcosystemBucket pypi_;
    EcosystemBucket cargo_;
    EcosystemBucket golang_;
    EcosystemBucket maven_;
    EcosystemBucket gem_;
    EcosystemBucket generic_;
};

} // namespace euxis::slopsquatting
