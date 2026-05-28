/// @file
/// @brief Provider failover with credential pool + error taxonomy.
///
/// Mirrors the Hermes Agent agent/credential_pool.py + error_classifier.py +
/// retry_utils.py pattern. The pieces:
///
///   - FailoverReason   : small taxonomy of "why did the provider call fail"
///   - RecoveryAction   : what the caller should do about it
///   - classify_failure : HTTP-status + message string -> FailoverReason
///   - CredentialPool   : round-robin pool of API keys with per-key cooldown
///   - jittered_backoff_ms : deterministic exponential backoff with jitter
///
/// The point is to give a production deployment a clean recovery story when
/// it hits 401/402/429 — rotate to the next key, back off and retry, or
/// surface to the user. This is also a FinOps proof point: rotation across
/// keys demonstrates the cost-optimisation claim is real.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace euxis::core {

/// @brief Why a provider call failed.
///
/// Small on purpose — callers want a recovery action, not a stack trace.
enum class FailoverReason {
    None,
    Auth,             ///< 401 — credential is bad; rotate to next key.
    Billing,          ///< 402 — quota / payment exhausted; rotate or abort.
    RateLimit,        ///< 429 — back off, retry same credential.
    ContextOverflow,  ///< 400 with "context too long" / "max_tokens" cues.
    ServerError,      ///< 5xx — retry with backoff.
    ModelNotFound,    ///< 404 or "model_not_found"; switch model.
    Network,          ///< Connection error; retry with backoff.
    Unknown,          ///< Anything else; surface to user, do not retry.
};

/// @brief Suggested recovery action for a classified failure.
enum class RecoveryAction {
    Abort,             ///< Surface to user.
    Retry,             ///< Retry with backoff, same credential.
    RotateCredential,  ///< Try next credential in the pool.
    CompactContext,    ///< Run context engine before retry.
    SwitchModel,       ///< Fall back to a different model.
};

/// @brief Classify a provider call failure from HTTP status + error text.
///
/// The status is the primary signal; the message is consulted only when
/// the status is ambiguous (e.g. 400 can mean context overflow OR a
/// malformed request).
[[nodiscard]] auto classify_failure(int http_status,
                                     std::string_view error_message) noexcept
    -> FailoverReason;

/// @brief Recommended recovery for a given FailoverReason.
[[nodiscard]] auto recovery_for(FailoverReason r) noexcept -> RecoveryAction;

/// @brief Stable lowercase name for telemetry / logging.
[[nodiscard]] auto reason_name(FailoverReason r) noexcept -> std::string_view;

/// @brief One credential entry with per-key cooldown state.
struct Credential {
    std::string  id;                       ///< Caller-supplied stable id.
    std::string  secret;                   ///< API key or bearer token.
    std::int64_t cooldown_until_unix_ms{0}; ///< 0 means available now.
};

/// @brief Round-robin pool of credentials for one provider.
///
/// Per-key cooldown is set explicitly via cool_down(). next_available()
/// skips any credential whose cooldown_until > now; if every credential
/// is cooled down, returns nullopt and the caller surfaces the error.
///
/// Thread-safe via a single internal mutex; the round-robin counter is
/// atomic so concurrent next_available() calls don't all see the same
/// starting index.
class CredentialPool {
public:
    /// @brief Construct a pool from a non-empty vector of credentials.
    explicit CredentialPool(std::vector<Credential> creds);

    /// @brief Get the next available credential id, advancing the
    ///        round-robin pointer.
    /// @return The id, or nullopt if every credential is still cooled down.
    [[nodiscard]] auto next_available(std::int64_t now_unix_ms) noexcept
        -> std::optional<std::string>;

    /// @brief Cool a credential down for @p duration_ms from @p now_unix_ms.
    /// @return true if @p id was found and cooled, false otherwise.
    auto cool_down(std::string_view id,
                   std::int64_t now_unix_ms,
                   std::int64_t duration_ms) noexcept -> bool;

    /// @brief Clear the cooldown on a credential (signal it's healthy).
    /// @return true if @p id was found, false otherwise.
    auto mark_healthy(std::string_view id) noexcept -> bool;

    /// @brief Total credentials in the pool (does not consider cooldown).
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /// @brief How many credentials are available right now.
    [[nodiscard]] auto available_count(std::int64_t now_unix_ms) const noexcept
        -> std::size_t;

private:
    std::vector<Credential>     creds_;
    std::atomic<std::size_t>    rr_{0};
    mutable std::mutex          mu_;
};

/// @brief Jittered exponential backoff with deterministic seed.
///
/// Schedule: delay = min(cap, base << min(attempt, 30)). Jitter is
/// uniform in [0, delay] using splitmix64 over (seed, attempt) so
/// callers can reproduce the same sequence in tests. When @p seed is 0
/// the function derives a non-deterministic seed from the current
/// system clock — production behaviour.
///
/// @param attempt 0-based retry counter.
/// @param base_ms Initial backoff (default 250 ms).
/// @param cap_ms  Maximum backoff (default 30 s).
/// @param seed    0 = non-deterministic, anything else = deterministic.
[[nodiscard]] auto jittered_backoff_ms(std::size_t attempt,
                                        std::int64_t base_ms = 250,
                                        std::int64_t cap_ms  = 30'000,
                                        std::uint64_t seed   = 0) noexcept
    -> std::int64_t;

} // namespace euxis::core
