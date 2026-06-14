/// @file
/// @brief Structured CLI exit codes (referenced from docs/reference/exit-codes.md).
///
/// Replaces the legacy binary 0/1 contract so CI gates can distinguish
/// infrastructure errors from advisory vs. blocking findings.
#pragma once

#include <cstdint>

namespace euxis::cli {

/// Canonical exit codes for the euxis CLI binary.
///
/// Code values are stable; callers may depend on the integer
/// representation. See docs/reference/exit-codes.md for CI worked
/// examples.
enum class ExitCode : std::uint8_t {
    /// Scan completed; no findings at or above the advisory threshold.
    Success = 0,
    /// Infrastructure error: missing config, parse failure, network
    /// error, internal panic. Treated as a real CI failure.
    InfraError = 1,
    /// Findings exist, all below the blocking_threshold. CI may emit
    /// warnings but should not fail the job.
    AdvisoryFindings = 2,
    /// At least one finding at or above the blocking_threshold.
    BlockingFindings = 3,
    /// Policy violation: signed-evidence verification failed,
    /// baseline tampered, ruleset signature invalid.
    PolicyViolation = 4,
    /// Time budget exceeded with partial results emitted.
    TimeoutPartial = 5,
    /// Interrupted (SIGINT). 128 + signal number convention.
    Interrupted = 130,
};

/// Convenience: cast to the underlying integer for return statements.
[[nodiscard]] constexpr auto to_int(ExitCode c) noexcept -> int {
    return static_cast<int>(c);
}

} // namespace euxis::cli
