/// @file
/// @brief Per-session token, cost, and provider-usage insights.
///
/// Mirrors the Hermes Agent agent/insights.py + usage_pricing.py +
/// account_usage.py pattern: a small record type per provider call, a
/// rollup function that produces a per-session summary, and a static
/// pricing table for the major providers. The point is to make Euxis's
/// FinOps positioning falsifiable — without per-session cost numbers
/// the "the router picks the cheapest model that still meets your SLO"
/// claim is unprovable.
///
/// Pricing in this table is the *current* posted rate as of the cutoff
/// (2026-05-27). Unknown provider + model combinations return zero
/// pricing rather than a fake number — callers see explicit "we don't
/// know" instead of silently fabricated cost.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace euxis::metrics {

/// @brief One token-usage record per provider call.
///
/// `cached_input_tokens` is the Anthropic-style cache-read count — the
/// subset of input_tokens that were served from the prompt cache (much
/// cheaper than fresh input). Cost is split accordingly in estimate_cost().
struct UsageRecord {
    std::string  session_id;
    std::string  agent_id;
    std::string  provider;              ///< "anthropic", "openai", "google", "ollama"
    std::string  model;                 ///< Provider-qualified model id
    std::size_t  input_tokens{0};
    std::size_t  cached_input_tokens{0};
    std::size_t  output_tokens{0};
    std::int64_t timestamp_unix_ms{0};
    double       cost_usd{0.0};
};

/// @brief Static provider pricing in USD per *million* tokens.
///
/// Three rates: fresh input, cached input (Anthropic), and output. A
/// zero-priced entry (everything 0.0) is the sentinel for "unknown
/// pricing"; callers can detect it via `is_known()`.
struct ProviderPricing {
    std::string provider;
    std::string model;
    double      input_per_million{0.0};
    double      cached_input_per_million{0.0};
    double      output_per_million{0.0};

    /// @brief True when at least one rate is non-zero.
    [[nodiscard]] auto is_known() const noexcept -> bool {
        return input_per_million > 0.0
            || cached_input_per_million > 0.0
            || output_per_million > 0.0;
    }
};

/// @brief Compute cost in USD for one call.
///
/// Token counts are clamped to non-negative; cached_input_tokens is
/// capped at input_tokens (cached cannot exceed total input).
[[nodiscard]] auto estimate_cost(const ProviderPricing& pricing,
                                  std::size_t input_tokens,
                                  std::size_t cached_input_tokens,
                                  std::size_t output_tokens) noexcept -> double;

/// @brief Look up pricing for a known (provider, model) pair.
///
/// Matching is case-insensitive on the provider and exact on the model.
/// Unknown combinations return a zero-priced ProviderPricing — callers
/// must check `is_known()` before drawing FinOps conclusions.
///
/// Seeded with current (2026-05-27) rates for:
///   anthropic: claude-opus-4-7, claude-sonnet-4-6, claude-haiku-4-5
///   openai:    gpt-5, gpt-5-mini
///   google:    gemini-2.5-pro, gemini-2.5-flash
///   ollama:    "*" (local, no cost)
[[nodiscard]] auto lookup_pricing(std::string_view provider,
                                   std::string_view model) noexcept
    -> ProviderPricing;

/// @brief Aggregated per-session rollup.
///
/// `per_model_cost` keys are "<provider>:<model>"; values are USD spent
/// against that model in this session. Order of insertion is preserved
/// so output is stable across calls.
struct SessionInsights {
    std::string session_id;
    std::size_t total_calls{0};
    std::size_t total_input_tokens{0};
    std::size_t total_cached_input_tokens{0};
    std::size_t total_output_tokens{0};
    double      total_cost_usd{0.0};
    std::vector<std::pair<std::string, double>> per_model_cost;
};

/// @brief Aggregate UsageRecord entries into a per-session summary.
///
/// Filters by session_id (records with a different session_id are
/// skipped). Pure function — no I/O, no global state. Safe to call on
/// any thread.
[[nodiscard]] auto aggregate(std::string_view session_id,
                              const std::vector<UsageRecord>& records)
    -> SessionInsights;

} // namespace euxis::metrics
