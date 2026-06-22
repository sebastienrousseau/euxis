/// @file
/// @brief Implementation of per-session token-cost insights.

#include "euxis/metrics/insights.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace euxis::metrics {

namespace {

// USD per million tokens. Verified current as of 2026-05-27.
struct PriceRow {
    std::string_view provider;
    std::string_view model;
    double           input_per_million;
    double           cached_input_per_million;
    double           output_per_million;
};

constexpr std::array<PriceRow, 8> kPriceTable{{
    {"anthropic", "claude-opus-4-7",    15.00, 1.50, 75.00},
    {"anthropic", "claude-sonnet-4-6",   3.00, 0.30, 15.00},
    {"anthropic", "claude-haiku-4-5",    1.00, 0.10,  5.00},
    {"openai",    "gpt-5",              12.00, 1.20, 60.00},
    {"openai",    "gpt-5-mini",          0.30, 0.03,  1.50},
    {"google",    "gemini-2.5-pro",      1.25, 0.31, 10.00},
    {"google",    "gemini-2.5-flash",    0.075, 0.019, 0.30},
    {"ollama",    "*",                   0.00, 0.00,  0.00},
}};

[[nodiscard]] auto to_lower(std::string_view s) -> std::string {
    std::string out;
    out.resize(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        out[i] = static_cast<char>(std::tolower(c));
    }
    return out;
}

} // namespace

auto estimate_cost(const ProviderPricing& pricing,
                   std::size_t input_tokens,
                   std::size_t cached_input_tokens,
                   std::size_t output_tokens) noexcept -> double {
    // Cap cached at total input — cached can never exceed input.
    if (cached_input_tokens > input_tokens) {
        cached_input_tokens = input_tokens;
    }
    const std::size_t fresh_input_tokens =
        input_tokens - cached_input_tokens;

    constexpr double kPerMillion = 1'000'000.0;
    const double cost_fresh =
        static_cast<double>(fresh_input_tokens) / kPerMillion
            * pricing.input_per_million;
    const double cost_cached =
        static_cast<double>(cached_input_tokens) / kPerMillion
            * pricing.cached_input_per_million;
    const double cost_output =
        static_cast<double>(output_tokens) / kPerMillion
            * pricing.output_per_million;

    return cost_fresh + cost_cached + cost_output;
}

auto lookup_pricing(std::string_view provider, std::string_view model)
    -> ProviderPricing {
    const std::string p = to_lower(provider);
    for (const auto& row : kPriceTable) {
        if (row.provider != std::string_view{p}) continue;
        // "*" is a wildcard for providers like ollama where any model is free.
        if (row.model == "*" || row.model == model) {
            ProviderPricing out;
            out.provider                  = std::string{row.provider};
            out.model                     = (row.model == "*")
                                                ? std::string{model}
                                                : std::string{row.model};
            out.input_per_million         = row.input_per_million;
            out.cached_input_per_million  = row.cached_input_per_million;
            out.output_per_million        = row.output_per_million;
            return out;
        }
    }
    // Unknown combination — explicit zero-priced sentinel.
    ProviderPricing out;
    out.provider = std::string{provider};
    out.model    = std::string{model};
    return out;
}

auto aggregate(std::string_view session_id,
               const std::vector<UsageRecord>& records) -> SessionInsights {
    SessionInsights ins;
    ins.session_id = std::string{session_id};

    for (const auto& r : records) {
        if (r.session_id != session_id) continue;
        ins.total_calls               += 1;
        ins.total_input_tokens        += r.input_tokens;
        ins.total_cached_input_tokens += r.cached_input_tokens;
        ins.total_output_tokens       += r.output_tokens;
        ins.total_cost_usd            += r.cost_usd;

        // Insertion-order bucketing: linear scan keeps the vector small
        // and stable. For an audit tool, deterministic order is more
        // valuable than O(1) lookup; typical sessions touch 2-5 models.
        const std::string key = r.provider + ":" + r.model;
        auto it = std::find_if(ins.per_model_cost.begin(),
                               ins.per_model_cost.end(),
                               [&](const auto& kv) { return kv.first == key; });
        if (it == ins.per_model_cost.end()) {
            ins.per_model_cost.emplace_back(key, r.cost_usd);
        } else {
            it->second += r.cost_usd;
        }
    }
    return ins;
}

} // namespace euxis::metrics
