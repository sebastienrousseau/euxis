/// @file
/// @brief Implementation of provider failover with credential pool.

#include "euxis/core/credential_pool.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace euxis::core {

namespace {

[[nodiscard]] auto contains_ci(std::string_view hay, std::string_view needle) noexcept
    -> bool {
    if (needle.empty() || hay.size() < needle.size()) return false;
    for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(hay[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// splitmix64 — small, fast, deterministic hash for the jitter mixing step.
[[nodiscard]] auto splitmix64(std::uint64_t x) noexcept -> std::uint64_t {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

} // namespace

// ---------------------------------------------------------------------------
// classify_failure
// ---------------------------------------------------------------------------

auto classify_failure(int http_status, std::string_view msg) noexcept
    -> FailoverReason {
    if (http_status == 401) return FailoverReason::Auth;
    if (http_status == 402) return FailoverReason::Billing;
    if (http_status == 429) return FailoverReason::RateLimit;
    if (http_status == 404) {
        if (contains_ci(msg, "model")) return FailoverReason::ModelNotFound;
        return FailoverReason::Unknown;
    }
    if (http_status == 400) {
        // 400 is the catch-all for client errors. Two distinct sub-cases
        // matter: context-too-long (compact and retry) and model-not-found.
        if (contains_ci(msg, "context") ||
            contains_ci(msg, "max_tokens") ||
            contains_ci(msg, "too long") ||
            contains_ci(msg, "token limit")) {
            return FailoverReason::ContextOverflow;
        }
        if (contains_ci(msg, "model_not_found") ||
            contains_ci(msg, "model not found") ||
            contains_ci(msg, "model is deprecated")) {
            return FailoverReason::ModelNotFound;
        }
        return FailoverReason::Unknown;
    }
    if (http_status >= 500 && http_status < 600) {
        return FailoverReason::ServerError;
    }
    if (http_status == 0) {
        // No HTTP response at all — connection-level failure.
        return FailoverReason::Network;
    }
    return FailoverReason::Unknown;
}

auto recovery_for(FailoverReason r) noexcept -> RecoveryAction {
    switch (r) {
        case FailoverReason::None:            return RecoveryAction::Abort;
        case FailoverReason::Auth:            // both auth + billing rotate the cred
        case FailoverReason::Billing:         return RecoveryAction::RotateCredential;
        case FailoverReason::RateLimit:       return RecoveryAction::Retry;
        case FailoverReason::ContextOverflow: return RecoveryAction::CompactContext;
        case FailoverReason::ServerError:     return RecoveryAction::Retry;
        case FailoverReason::ModelNotFound:   return RecoveryAction::SwitchModel;
        case FailoverReason::Network:         return RecoveryAction::Retry;
        case FailoverReason::Unknown:         return RecoveryAction::Abort;
    }
    return RecoveryAction::Abort;
}

auto reason_name(FailoverReason r) noexcept -> std::string_view {
    switch (r) {
        case FailoverReason::None:            return "none";
        case FailoverReason::Auth:            return "auth";
        case FailoverReason::Billing:         return "billing";
        case FailoverReason::RateLimit:       return "rate_limit";
        case FailoverReason::ContextOverflow: return "context_overflow";
        case FailoverReason::ServerError:     return "server_error";
        case FailoverReason::ModelNotFound:   return "model_not_found";
        case FailoverReason::Network:         return "network";
        case FailoverReason::Unknown:         return "unknown";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// CredentialPool
// ---------------------------------------------------------------------------

CredentialPool::CredentialPool(std::vector<Credential> creds)
    : creds_{std::move(creds)} {}

auto CredentialPool::next_available(std::int64_t now_unix_ms) noexcept
    -> std::optional<std::string> {
    const std::scoped_lock lock{mu_};
    const std::size_t n = creds_.size();
    if (n == 0) return std::nullopt;

    // Walk the ring starting from the round-robin pointer; advance the
    // pointer past the chosen credential so the next caller starts
    // strictly past the last hit.
    const std::size_t start = rr_.fetch_add(1, std::memory_order_relaxed) % n;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = (start + i) % n;
        if (creds_[idx].cooldown_until_unix_ms <= now_unix_ms) {
            return creds_[idx].id;
        }
    }
    return std::nullopt;
}

auto CredentialPool::cool_down(std::string_view id,
                               std::int64_t now_unix_ms,
                               std::int64_t duration_ms) noexcept -> bool {
    const std::scoped_lock lock{mu_};
    for (auto& c : creds_) {
        if (c.id == id) {
            c.cooldown_until_unix_ms = now_unix_ms + duration_ms;
            return true;
        }
    }
    return false;
}

auto CredentialPool::mark_healthy(std::string_view id) noexcept -> bool {
    const std::scoped_lock lock{mu_};
    for (auto& c : creds_) {
        if (c.id == id) {
            c.cooldown_until_unix_ms = 0;
            return true;
        }
    }
    return false;
}

auto CredentialPool::size() const noexcept -> std::size_t {
    const std::scoped_lock lock{mu_};
    return creds_.size();
}

auto CredentialPool::available_count(std::int64_t now_unix_ms) const noexcept
    -> std::size_t {
    const std::scoped_lock lock{mu_};
    std::size_t n = 0;
    for (const auto& c : creds_) {
        if (c.cooldown_until_unix_ms <= now_unix_ms) ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// jittered_backoff_ms
// ---------------------------------------------------------------------------

auto jittered_backoff_ms(std::size_t attempt,
                         std::int64_t base_ms,
                         std::int64_t cap_ms,
                         std::uint64_t seed) noexcept -> std::int64_t {
    if (base_ms <= 0) return 0;
    if (cap_ms < base_ms) cap_ms = base_ms;

    // Compute the un-jittered ceiling.
    // Clamp the shift to 30 to avoid undefined behaviour on huge attempts.
    const std::size_t shift = (attempt > 30) ? 30 : attempt;
    std::int64_t scaled = base_ms;
    // Detect overflow before we hit it: if scaled * 2 would exceed cap,
    // pin to cap and stop early.
    for (std::size_t i = 0; i < shift; ++i) {
        if (scaled >= cap_ms / 2) { scaled = cap_ms; break; }
        scaled *= 2;
    }
    if (scaled > cap_ms) scaled = cap_ms;

    // Pick the jitter seed.
    std::uint64_t effective_seed = seed;
    if (effective_seed == 0) {
        effective_seed = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // Uniform jitter in [0, scaled].
    const std::uint64_t mix =
        splitmix64(effective_seed ^ (static_cast<std::uint64_t>(attempt) + 1));
    const std::int64_t jitter =
        static_cast<std::int64_t>(mix % static_cast<std::uint64_t>(scaled + 1));
    return jitter;
}

} // namespace euxis::core
