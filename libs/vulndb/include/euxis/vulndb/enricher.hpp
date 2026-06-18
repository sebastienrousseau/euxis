/// @file
/// @brief Cross-references an SBOM against OSV and emits `EnrichedReport`.

#pragma once

#include <chrono>
#include <expected>

#include "euxis/sbom/types.hpp"

#include "errors.hpp"
#include "osv_client.hpp"
#include "types.hpp"

namespace euxis::vulndb {

/// @brief Tunables for `Enricher::enrich`.
struct EnricherConfig {
    /// When true, emit a `VexStatement{NotAffected}` for components
    /// with zero matches. Useful for auditors who want positive proof
    /// of "we looked and found nothing". False by default to keep
    /// the VEX document small.
    bool emit_not_affected{false};
    /// Maximum components to enrich. 0 means unbounded. Bounded runs
    /// useful for CI smoke tests where the full SBOM would take minutes.
    std::size_t max_components{0};
    /// Per-component query timeout override (default uses OsvClient's).
    std::chrono::milliseconds per_query_timeout{0};
};

/// @brief Orchestrates the OSV cross-reference + VEX emission.
///
/// Stateless: a single `Enricher` instance can `enrich` multiple
/// SBOMs back-to-back without coordination.
class Enricher {
public:
    /// @brief Construct an enricher backed by the given OSV client.
    ///        The client must outlive the enricher.
    explicit Enricher(const OsvClient& client,
                      EnricherConfig   cfg = {});

    /// @brief Walk `doc.components`, query OSV for each, and assemble
    ///        an `EnrichedReport`.
    [[nodiscard]] auto enrich(const euxis::sbom::SbomDocument& doc) const
        -> std::expected<EnrichedReport, Error>;

    [[nodiscard]] auto config() const noexcept -> const EnricherConfig& {
        return cfg_;
    }

private:
    const OsvClient* client_;
    EnricherConfig   cfg_;
};

} // namespace euxis::vulndb
