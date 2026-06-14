/// @file
/// @brief Deterministic verifier — for tests and offline runs.
///
/// Returns true_positive when the supplied predicate fires on the
/// VoteRequest. Useful for two cases:
///
///   1. Unit tests, where we want the ensemble to behave
///      predictably without spinning up real LLM clients.
///   2. Offline / air-gapped runs, where a curated set of
///      heuristics (rule id allowlist, known CWE blocklist, …)
///      stands in for the multi-LLM ensemble.
///
/// In both cases the runner can mix Deterministic verifiers with
/// LLM verifiers — the EnsembleVote shape is identical.
#pragma once

#include <functional>
#include <string>

#include "euxis/ensemble/verifier.hpp"

namespace euxis::ensemble {

/// Predicate fed to a DeterministicVerifier. Returns true when the
/// verifier should vote `true_positive`. The verifier sets the
/// confidence and rationale from the configured defaults.
using DeterministicPredicate = std::function<bool(const VoteRequest&)>;

class DeterministicVerifier : public Verifier {
public:
    /// Construct with the provider id surfaced in EnsembleVote.
    /// `predicate` defaults to "always true positive" so the
    /// no-arg constructor is the canonical "always agrees"
    /// verifier used in confidence-escalation tests.
    explicit DeterministicVerifier(
        std::string provider_id,
        DeterministicPredicate predicate = [](const VoteRequest&) { return true; },
        float confidence_when_true       = 1.0F,
        float confidence_when_false      = 0.0F,
        std::string rationale_when_true  = "deterministic match",
        std::string rationale_when_false = "deterministic no-match");

    ~DeterministicVerifier() override = default;

    DeterministicVerifier(const DeterministicVerifier&)              = delete;
    DeterministicVerifier& operator=(const DeterministicVerifier&)   = delete;
    DeterministicVerifier(DeterministicVerifier&&) noexcept          = default;
    DeterministicVerifier& operator=(DeterministicVerifier&&) noexcept = default;

    [[nodiscard]] auto provider_id() const noexcept -> std::string override;
    [[nodiscard]] auto vote(const VoteRequest& req) -> VoteOutcome override;

private:
    std::string provider_id_;
    DeterministicPredicate predicate_;
    float confidence_when_true_;
    float confidence_when_false_;
    std::string rationale_when_true_;
    std::string rationale_when_false_;
};

} // namespace euxis::ensemble
