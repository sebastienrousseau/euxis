/// @file
/// @brief Deterministic verdict consistency rules and cross-fleet drift
///        detection for the playbook artifact.
///
/// These functions were originally defined inside `apps/cli/src/cmd/fleet.cpp`'s
/// anonymous namespace, which blocked unit testing. The bodies now live in
/// `playbook_verify.cpp` so tests can drive the R1–R5 verdict rules and the
/// historical drift detector directly, while fleet.cpp keeps using them via
/// thin adapters that convert its richer `AgentEvidence` to `PlaybookEvidence`.

#pragma once

#include <string>
#include <vector>

namespace euxis::cli::cmd {

/// POD subset of fleet.cpp's `AgentEvidence` carrying only the fields these
/// pure rules need. Keeping it minimal avoids coupling the verifier to the
/// full evidence object's growing list of execution-context fields.
struct PlaybookEvidence {
    std::string agent_name;
    std::string agent_id;
    std::string verdict;       // PASS / FAILED / TIMEOUT / PARTIAL / DEGRADED
    std::string raw_verdict;   // Original verdict pre-timeout classification
    double duration_ms{0.0};
};

/// A single rule failure produced by `verify_verdict_consistency`.
struct VerdictViolation {
    std::string rule;    // "R1".."R5"
    std::string detail;  // Human-readable explanation
};

/// Detector output from `detect_agent_drift`.
struct DriftAlert {
    std::string agent_id;
    std::string metric;        // "latency" | "verdict_distribution"
    double baseline_value{0.0};
    double current_value{0.0};
    double deviation_pct{0.0};
    std::string severity;      // "info" | "warning" | "alert"
};

/// Apply R1–R5 to the playbook's final verdict and report rule violations.
///   R1: TRUSTED requires no contradictions / fails / timeouts and conf ≥ 80.
///   R2: BLOCKED requires unanimous decisive failure.
///   R3: INCONCLUSIVE requires at least one contradiction.
///   R4: All-TIMEOUT cannot be TRUSTED.
///   R5: Majority decisive negatives cannot be TRUSTED / TRUSTED WITH GAPS.
[[nodiscard]] std::vector<VerdictViolation> verify_verdict_consistency(
    const std::string& verdict,
    const std::vector<PlaybookEvidence>& evidence,
    int confidence,
    int contradiction_count);

/// Compare current-run latencies against the last ≤10 runs' history.jsonl and
/// flag agents exceeding ±2σ on latency or transitioning from a low-timeout
/// baseline (<30%) into a timeout this run.
[[nodiscard]] std::vector<DriftAlert> detect_agent_drift(
    const std::string& euxis_home,
    const std::vector<PlaybookEvidence>& current_evidence);

} // namespace euxis::cli::cmd
