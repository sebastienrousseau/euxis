/// @file
/// @brief CodeX-Verify-style ensemble runner.
///
/// Given a set of Findings and a set of Verifiers, the runner asks
/// every verifier to vote on every finding, applies a quorum rule
/// to decide whether to confirm or reject, attaches the votes to
/// the Finding's existing ensemble_votes field (defined in
/// libs/security since Week 2), and adjusts the Finding's
/// confidence to reflect the ensemble outcome:
///
///   ratio >= quorum AND unanimous              → Verified
///   ratio >= quorum                            → Probable
///   ratio < quorum  AND votes > 0              → Possible
///   no votes (no verifiers)                    → unchanged
///
/// The runner deliberately does NOT delete findings. Even a
/// finding that fails the quorum is kept (with confidence
/// downgraded and votes attached) so a human reviewer can inspect
/// the disagreement.
#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "euxis/ensemble/verifier.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::ensemble {

struct EnsembleConfig {
    /// Minimum number of true_positive votes required to confirm
    /// a finding. Default: 2 (per CodeX-Verify recommendation).
    /// Must be ≥ 1.
    int quorum{2};

    /// When true, a finding is confirmed only if EVERY verifier
    /// voted true_positive. Useful for "all-or-nothing" auditing
    /// runs; off by default.
    bool require_unanimous{false};

    /// When true, the runner trims the snippet attached to each
    /// VoteRequest at this many bytes. Default 4096 — fits inside
    /// one Anthropic prompt comfortably.
    std::size_t max_snippet_bytes{4096};
};

struct EnsembleStats {
    std::size_t findings_verified{0};
    std::size_t verifiers_called{0};
    std::size_t confirmed{0};
    std::size_t rejected{0};
};

struct EnsembleResult {
    std::vector<euxis::security::Finding> findings;
    EnsembleStats stats;
};

/// Run the ensemble against `findings`. Returns the same findings
/// in the same order, each enriched with its ensemble_votes vector
/// and (possibly) a downgraded/upgraded confidence.
///
/// `verifiers` is a span of shared pointers so callers can build
/// the ensemble from a mix of stable verifiers (held in a member)
/// and ephemeral ones (constructed at call site).
[[nodiscard]] auto verify(std::vector<euxis::security::Finding> findings,
                          std::span<const VerifierPtr> verifiers,
                          const EnsembleConfig& config = {})
    -> EnsembleResult;

} // namespace euxis::ensemble
