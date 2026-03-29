/// @file
/// @brief Compile-time verdict guardrails for the euxis verdict engine.
#pragma once

#include <cstdint>
#include <string_view>

namespace euxis::cli::verdict {

/// Terminal verdict states, ordered by severity.
enum class State : uint8_t {
    Inconclusive = 0,
    Blocked = 1,
    Caution = 2,
    TrustedGaps = 3,
    Trusted = 4
};

/// Agent-level states.
enum class AgentState : uint8_t {
    Pass = 0,
    Partial = 1,
    Timeout = 2,
    Failed = 3,
    Degraded = 4,
    Contradicted = 5
};

/// Convert verdict state to display string.
constexpr auto to_string(State s) -> std::string_view {
    switch (s) {
        case State::Inconclusive: return "INCONCLUSIVE";
        case State::Blocked:      return "BLOCKED";
        case State::Caution:      return "CAUTION";
        case State::TrustedGaps:  return "TRUSTED WITH GAPS";
        case State::Trusted:      return "TRUSTED";
    }
    return "UNKNOWN";
}

/// Convert agent state to display string.
constexpr auto to_string(AgentState s) -> std::string_view {
    switch (s) {
        case AgentState::Pass:         return "PASS";
        case AgentState::Partial:      return "PARTIAL";
        case AgentState::Timeout:      return "TIMEOUT";
        case AgentState::Failed:       return "FAILED";
        case AgentState::Degraded:     return "DEGRADED";
        case AgentState::Contradicted: return "CONTRADICTED";
    }
    return "UNKNOWN";
}

/// Classify raw verdict string to agent state.
constexpr auto classify_agent(std::string_view raw, bool timed_out, bool degraded) -> AgentState {
    if (timed_out) return AgentState::Timeout;
    if (degraded) return AgentState::Degraded;
    if (raw == "PASS") return AgentState::Pass;
    if (raw == "FAIL") return AgentState::Failed;
    return AgentState::Partial;
}

/// Check if an agent state counts as a decisive negative.
constexpr auto is_decisive_negative(AgentState state, std::string_view raw_verdict) -> bool {
    if (state == AgentState::Failed) return true;
    // TIMEOUT with raw FAIL is still a negative signal
    if (state == AgentState::Timeout && raw_verdict == "FAIL") return true;
    return false;
}

/// Numeric rank for ordering.
constexpr auto rank(State s) -> int {
    return static_cast<int>(s);
}

/// Map verdict to exit code.
constexpr auto exit_code(State s) -> int {
    switch (s) {
        case State::Trusted:
        case State::TrustedGaps:
            return 0;
        case State::Blocked:
        case State::Inconclusive:
        case State::Caution:
            return 1;
    }
    return 2;
}

/// Apply escalation cap — escalation can only demote, never promote.
constexpr auto apply_escalation_cap(State current, State capped_at) -> State {
    return (rank(current) > rank(capped_at)) ? capped_at : current;
}

/// Validate verdict invariants.
/// Returns true if the verdict is consistent with the given evidence parameters.
constexpr auto is_valid_verdict(State verdict, int effective_fails, int timeout_count,
                                 bool has_contradiction, int confidence,
                                 int total_agents) -> bool {
    switch (verdict) {
        case State::Trusted:
            // Trusted requires: no contradictions, no fails, no timeouts, confidence >= 80
            if (has_contradiction) return false;
            if (effective_fails > 0) return false;
            if (timeout_count > 0) return false;
            if (confidence < 80) return false;
            return true;

        case State::Blocked:
            // Blocked requires failures
            if (effective_fails == 0) return false;
            return true;

        case State::Inconclusive:
            // Inconclusive requires contradictions
            if (!has_contradiction) return false;
            return true;

        case State::TrustedGaps:
            // Cannot have majority decisive negatives
            if (total_agents > 0 && effective_fails > total_agents / 2) return false;
            return true;

        case State::Caution:
            return true;  // Caution is always valid
    }
    return false;
}

// --- Compile-time proofs via static_assert ---

// Proof: Trusted impossible with contradictions
static_assert(!is_valid_verdict(State::Trusted, 0, 0, true, 100, 5),
              "Trusted must not be valid with contradictions");

// Proof: Trusted impossible with fails
static_assert(!is_valid_verdict(State::Trusted, 1, 0, false, 100, 5),
              "Trusted must not be valid with effective fails");

// Proof: Trusted impossible with timeouts
static_assert(!is_valid_verdict(State::Trusted, 0, 1, false, 100, 5),
              "Trusted must not be valid with timeouts");

// Proof: Trusted impossible with low confidence
static_assert(!is_valid_verdict(State::Trusted, 0, 0, false, 50, 5),
              "Trusted must not be valid with confidence < 80");

// Proof: Blocked impossible without fails
static_assert(!is_valid_verdict(State::Blocked, 0, 0, false, 50, 5),
              "Blocked must not be valid without fails");

// Proof: Inconclusive requires contradiction
static_assert(!is_valid_verdict(State::Inconclusive, 0, 0, false, 50, 5),
              "Inconclusive must not be valid without contradictions");

// Proof: Escalation cap never promotes
static_assert(rank(apply_escalation_cap(State::Trusted, State::Caution)) <= rank(State::Caution),
              "Escalation cap must never promote beyond cap");

// Proof: Exit codes consistent
static_assert(exit_code(State::Trusted) == 0, "Trusted exit code must be 0");
static_assert(exit_code(State::Blocked) == 1, "Blocked exit code must be 1");
static_assert(exit_code(State::Inconclusive) == 1, "Inconclusive exit code must be 1");

} // namespace euxis::cli::verdict
