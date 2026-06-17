#include "euxis/slopsquatting/integration.hpp"

#include <array>
#include <cstdint>
#include <sstream>

#include "euxis/sca/scanner.hpp"

namespace euxis::slopsquatting {

namespace {

using euxis::security::CweRef;
using euxis::security::Finding;
using euxis::security::OwaspCategory;
using euxis::security::RuleSource;
using euxis::security::Severity;
using euxis::security::SourceLocation;

constexpr const char* kRuleId = "euxis/slopsquatting/EUXIS-SLOPSQ-001";

// 1:1 identity mapping between two parallel enums; each case is
// documentation that the link between Ecosystem and PurlType is
// explicit (and audited), not the result of a default-int cast.
// bugprone-branch-clone correctly notes the branches are
// structurally identical at the integer level — that is the
// invariant we want to preserve.
// NOLINTBEGIN(bugprone-branch-clone)
auto ecosystem_to_purl_type(euxis::sca::Ecosystem e) noexcept
    -> euxis::sbom::PurlType {
    switch (e) {
        case euxis::sca::Ecosystem::Cargo:    return euxis::sbom::PurlType::Cargo;
        case euxis::sca::Ecosystem::Npm:      return euxis::sbom::PurlType::Npm;
        case euxis::sca::Ecosystem::Pypi:     return euxis::sbom::PurlType::Pypi;
        case euxis::sca::Ecosystem::Golang:   return euxis::sbom::PurlType::Golang;
        case euxis::sca::Ecosystem::Maven:    return euxis::sbom::PurlType::Maven;
        case euxis::sca::Ecosystem::Gem:      return euxis::sbom::PurlType::Gem;
        case euxis::sca::Ecosystem::Composer: return euxis::sbom::PurlType::Composer;
    }
    return euxis::sbom::PurlType::Generic;
}
// NOLINTEND(bugprone-branch-clone)

/// Produce a stable fingerprint that survives line moves and
/// whitespace edits. The components of the hash are deterministic:
/// rule id + normalised lockfile path + ecosystem + package name.
/// FNV-1a over the concatenation gives a 64-bit hash; rendered as
/// 32 hex chars by duplicating to fill the BLAKE2b-128 shape the
/// canonical Finding expects.
auto stable_fingerprint(const std::string& path,
                        std::string_view ecosystem,
                        std::string_view name) -> std::string {
    auto fnv1a = [](std::string_view s, std::uint64_t seed) {
        std::uint64_t h = seed;
        for (unsigned char c : s) {
            h ^= c;
            h *= 0x100000001b3ULL;
        }
        return h;
    };

    std::uint64_t h = 0xcbf29ce484222325ULL;
    h = fnv1a("EUXIS-SLOPSQ-001", h);
    h = fnv1a(path, h);
    h = fnv1a(ecosystem, h);
    h = fnv1a(name, h);

    static constexpr std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    std::string out;
    out.reserve(32);
    // Duplicate hash to fill 128 bits; collisions are bounded by the
    // 64-bit input space, which is acceptable for baseline matching.
    auto append = [&](std::uint64_t v) {
        for (int i = 15; i >= 0; --i) {
            out.push_back(hex[(v >> (i * 4)) & 0xF]);
        }
    };
    append(h);
    append(h ^ 0x5a5a5a5a5a5a5a5aULL);
    return out;
}

} // namespace

auto check_scan_result(const Guard& guard,
                       const euxis::sca::ScanResult& scan)
    -> std::vector<Finding> {
    std::vector<Finding> findings;

    for (const auto& m : scan.manifests) {
        const auto purl_type = ecosystem_to_purl_type(m.ecosystem);
        const auto eco_name  = std::string{euxis::sca::ecosystem_str(m.ecosystem)};
        const auto file_path = m.source_file.string();

        for (const auto& entry : m.entries) {
            // Match against the canonical name the registry would
            // resolve. For npm scoped pkgs the ns is the @scope.
            std::string lookup_name = entry.ns.empty()
                ? entry.name
                : entry.ns + "/" + entry.name;
            if (!guard.contains(lookup_name, purl_type)) {
                // Fall back to bare name for ecosystems that do not
                // namespace at the registry level (cargo, gem, etc.)
                if (!guard.contains(entry.name, purl_type)) continue;
                lookup_name = entry.name;
            }

            Finding f;
            f.rule_id = kRuleId;
            f.severity = Severity::High;
            f.confidence = euxis::security::Confidence::Probable;
            f.source = RuleSource::SlopsquattingGuard;
            f.owasp = OwaspCategory::A03_SupplyChainFailures;
            f.cwe = CweRef{
                .id = "CWE-1357",
                .short_name = "Reliance on Insufficiently Trustworthy Component",
                .in_top_25_2025 = false,
                .top_25_rank = 0,
            };

            std::ostringstream msg;
            msg << "Package '" << lookup_name
                << "' (" << eco_name << ") is in the slopsquatting "
                   "hallucinated-package corpus. LLM-generated code "
                   "frequently names this package even though the "
                   "legitimate registry entry does not exist (or "
                   "exists and is materially different). Verify the "
                   "import is intentional before resolving against "
                   "the public registry.";
            f.message = msg.str();

            f.primary_location = SourceLocation{
                .path = file_path,
                .start_line = 0,
                .start_column = 0,
                .end_line = 0,
                .end_column = 0,
                .snippet = entry.name,
            };
            f.compliance_taxa = {
                "owasp:A03:2025",
                "cwe:CWE-1357",
                "cra:annex-i:1.3.b",
                "nist-ssdf:PW.4.1",
            };
            f.stable_fingerprint = stable_fingerprint(file_path, eco_name, lookup_name);
            findings.push_back(std::move(f));
        }
    }
    return findings;
}

} // namespace euxis::slopsquatting
