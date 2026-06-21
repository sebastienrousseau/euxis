/// @file
/// @brief Abstract verifier base + the request payload.
///
/// A `Verifier` answers the question "is this Finding a true
/// positive?" — typically by calling out to an LLM, but the base
/// class is agnostic so tests can plug a deterministic mock in.
///
/// The ensemble runner in runner.hpp calls every Verifier per
/// Finding and combines their votes via a quorum rule. The shape
/// of the request is intentionally small (rule id, message, source
/// snippet, location) so a verifier implementation can compose its
/// own prompt without dragging the full Finding type across the
/// network.
///
/// Reference: CodeX-Verify (arXiv:2511.16708) — combining
/// conditionally-independent agents via majority vote beats any
/// single agent by +39.7pp on the SastBench triage benchmark.
#pragma once

#include <memory>
#include <string>

#include "euxis/security/finding.hpp"

namespace euxis::ensemble {

/// Payload sent to each Verifier. Kept small so a real provider
/// can fit it inside one API call. The runner constructs one of
/// these per Finding before fanning out.
struct VoteRequest {
    /// The rule that fired. Verifiers may use this to weight their
    /// confidence (e.g. high-severity SQL injection rules get
    /// stricter checking than informational lint rules).
    std::string rule_id;

    /// Human-readable description from the originating rule. The
    /// verifier sees the same text the user would.
    std::string message;

    /// Source snippet around the finding. The runner crops at the
    /// primary_location's range; verifiers can ask for more context
    /// by walking related_locations themselves if needed.
    std::string snippet;

    /// File path the finding was emitted in. Surfaced in the prompt
    /// for grounding ("does this look right for a file under
    /// docs/examples/?") and persisted in the resulting Finding.
    std::string source_path;

    /// 1-based row in the source file (matches SARIF).
    int start_row{0};
    int start_column{0};

    /// Severity assigned by the originating engine. Forwarded as a
    /// hint; the verifier is free to disagree.
    euxis::security::Severity severity{euxis::security::Severity::Medium};
};

/// Verdict returned by a single Verifier. Carries the canonical
/// `EnsembleVote` shape so the runner can attach it to the Finding
/// without re-wrapping.
struct VoteOutcome {
    euxis::security::Finding::EnsembleVote vote;
    /// Optional free-form rationale. Persisted alongside the vote
    /// so a human reviewer can see why a verifier voted as it did.
    /// Empty by default for the deterministic mock.
    std::string rationale;
};

/// Abstract verifier. One per "voter" in the ensemble. Concrete
/// implementations may call out to LLMs (Claude, OpenAI, Gemini),
/// run deterministic checks against a knowledge base, or read from
/// cached prior outcomes.
class Verifier {
public:
    virtual ~Verifier() = default;
    Verifier()                              = default;
    Verifier(const Verifier&)              = delete;
    Verifier& operator=(const Verifier&)   = delete;
    Verifier(Verifier&&) noexcept          = default;
    Verifier& operator=(Verifier&&) noexcept = default;

    /// Stable identifier the runner stores in the EnsembleVote.
    /// e.g. "claude-opus-4-7", "deterministic/always-true",
    /// "static/known-cwe-79".
    [[nodiscard]] virtual auto provider_id() const -> std::string = 0;

    /// Cast the vote. Implementations must be thread-safe — the
    /// runner may call multiple verifiers concurrently in a
    /// future batch (the Week 15 runner is synchronous).
    [[nodiscard]] virtual auto vote(const VoteRequest& req) -> VoteOutcome = 0;
};

/// Convenience shared-ownership handle used everywhere Verifier
/// instances are passed around. Avoids the lifetime gymnastics that
/// would otherwise force every caller to manage the void slot.
using VerifierPtr = std::shared_ptr<Verifier>;

} // namespace euxis::ensemble
