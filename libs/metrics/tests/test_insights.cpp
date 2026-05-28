/// @file
/// @brief Tests for per-session insights and provider pricing lookup.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "euxis/metrics/insights.hpp"

namespace euxis::metrics {
namespace {

// ---------------------------------------------------------------------------
// estimate_cost
// ---------------------------------------------------------------------------

TEST(EstimateCostTest, AllZeroTokensProducesZero) {
    ProviderPricing p{"anthropic", "claude-opus-4-7", 15.00, 1.50, 75.00};
    EXPECT_DOUBLE_EQ(estimate_cost(p, 0, 0, 0), 0.0);
}

TEST(EstimateCostTest, InputOnlyComputesCorrectCost) {
    ProviderPricing p{"anthropic", "claude-opus-4-7", 15.00, 1.50, 75.00};
    // 1M input @ $15/M  → $15.00
    EXPECT_DOUBLE_EQ(estimate_cost(p, 1'000'000, 0, 0), 15.00);
}

TEST(EstimateCostTest, CachedTokensReducePrice) {
    ProviderPricing p{"anthropic", "claude-opus-4-7", 15.00, 1.50, 75.00};
    // 1M input total, 900k cached → 100k fresh + 900k cached + 0 output.
    // Fresh:  0.1 * 15.00 = $1.50
    // Cached: 0.9 *  1.50 = $1.35
    // Total = $2.85
    EXPECT_DOUBLE_EQ(estimate_cost(p, 1'000'000, 900'000, 0), 2.85);
}

TEST(EstimateCostTest, OutputOnlyComputesCorrectCost) {
    ProviderPricing p{"anthropic", "claude-opus-4-7", 15.00, 1.50, 75.00};
    // 500k output @ $75/M → $37.50
    EXPECT_DOUBLE_EQ(estimate_cost(p, 0, 0, 500'000), 37.50);
}

TEST(EstimateCostTest, CachedCappedAtInputTotal) {
    ProviderPricing p{"anthropic", "claude-opus-4-7", 15.00, 1.50, 75.00};
    // Passing cached > input should clamp cached to input (no negative
    // fresh tokens). 100 total, 99999 "cached" → treats as 100 cached.
    const double cost  = estimate_cost(p, 100, 99999, 0);
    const double expect = 100.0 / 1'000'000.0 * 1.50;
    EXPECT_DOUBLE_EQ(cost, expect);
}

// ---------------------------------------------------------------------------
// lookup_pricing
// ---------------------------------------------------------------------------

TEST(LookupPricingTest, AnthropicOpus47IsKnown) {
    auto p = lookup_pricing("anthropic", "claude-opus-4-7");
    EXPECT_TRUE(p.is_known());
    EXPECT_DOUBLE_EQ(p.input_per_million,        15.00);
    EXPECT_DOUBLE_EQ(p.cached_input_per_million,  1.50);
    EXPECT_DOUBLE_EQ(p.output_per_million,       75.00);
}

TEST(LookupPricingTest, AnthropicSonnet46IsKnown) {
    auto p = lookup_pricing("anthropic", "claude-sonnet-4-6");
    EXPECT_TRUE(p.is_known());
    EXPECT_DOUBLE_EQ(p.input_per_million,         3.00);
    EXPECT_DOUBLE_EQ(p.output_per_million,       15.00);
}

TEST(LookupPricingTest, OpenAIAndGoogleAreKnown) {
    EXPECT_TRUE(lookup_pricing("openai", "gpt-5").is_known());
    EXPECT_TRUE(lookup_pricing("openai", "gpt-5-mini").is_known());
    EXPECT_TRUE(lookup_pricing("google", "gemini-2.5-pro").is_known());
    EXPECT_TRUE(lookup_pricing("google", "gemini-2.5-flash").is_known());
}

TEST(LookupPricingTest, OllamaWildcardMatchesAnyModelAsFree) {
    auto p = lookup_pricing("ollama", "llama3.3:70b");
    EXPECT_FALSE(p.is_known()) << "all rates zero → not known";
    EXPECT_EQ(p.provider, "ollama");
    EXPECT_EQ(p.model,    "llama3.3:70b");
}

TEST(LookupPricingTest, ProviderMatchingIsCaseInsensitive) {
    EXPECT_TRUE(lookup_pricing("Anthropic", "claude-opus-4-7").is_known());
    EXPECT_TRUE(lookup_pricing("ANTHROPIC", "claude-opus-4-7").is_known());
}

TEST(LookupPricingTest, UnknownProviderReturnsZeroPricing) {
    auto p = lookup_pricing("madeup-provider", "any-model");
    EXPECT_FALSE(p.is_known());
    EXPECT_EQ(p.provider, "madeup-provider");
    EXPECT_EQ(p.model,    "any-model");
    EXPECT_DOUBLE_EQ(p.input_per_million,        0.0);
    EXPECT_DOUBLE_EQ(p.output_per_million,       0.0);
}

TEST(LookupPricingTest, KnownProviderUnknownModelReturnsZeroPricing) {
    auto p = lookup_pricing("anthropic", "claude-future-model-9000");
    EXPECT_FALSE(p.is_known());
}

// ---------------------------------------------------------------------------
// aggregate
// ---------------------------------------------------------------------------

TEST(AggregateTest, EmptyRecordsProducesEmptyInsights) {
    auto ins = aggregate("s1", {});
    EXPECT_EQ(ins.session_id, "s1");
    EXPECT_EQ(ins.total_calls,         0u);
    EXPECT_EQ(ins.total_input_tokens,  0u);
    EXPECT_EQ(ins.total_output_tokens, 0u);
    EXPECT_DOUBLE_EQ(ins.total_cost_usd, 0.0);
    EXPECT_TRUE(ins.per_model_cost.empty());
}

TEST(AggregateTest, SumsTokensAndCostAcrossRecords) {
    std::vector<UsageRecord> records;
    records.push_back({
        .session_id = "s1", .agent_id = "a", .provider = "anthropic",
        .model = "claude-opus-4-7",
        .input_tokens = 100, .cached_input_tokens = 50, .output_tokens = 200,
        .timestamp_unix_ms = 0, .cost_usd = 1.0,
    });
    records.push_back({
        .session_id = "s1", .agent_id = "a", .provider = "anthropic",
        .model = "claude-opus-4-7",
        .input_tokens = 300, .cached_input_tokens = 100, .output_tokens = 400,
        .timestamp_unix_ms = 1, .cost_usd = 2.5,
    });

    auto ins = aggregate("s1", records);
    EXPECT_EQ(ins.total_calls,                2u);
    EXPECT_EQ(ins.total_input_tokens,         400u);
    EXPECT_EQ(ins.total_cached_input_tokens,  150u);
    EXPECT_EQ(ins.total_output_tokens,        600u);
    EXPECT_DOUBLE_EQ(ins.total_cost_usd, 3.5);
}

// Default-construct then assign — avoids -Werror=missing-field-initializers
// on partial designated initializers under GCC.
auto make_record(std::string session, std::string provider, std::string model,
                 double cost) -> UsageRecord {
    UsageRecord r{};
    r.session_id = std::move(session);
    r.provider   = std::move(provider);
    r.model      = std::move(model);
    r.cost_usd   = cost;
    return r;
}

TEST(AggregateTest, BucketsCostPerModelStably) {
    std::vector<UsageRecord> records;
    records.push_back(make_record("s1", "anthropic", "claude-opus-4-7", 1.0));
    records.push_back(make_record("s1", "openai",    "gpt-5",           2.0));
    records.push_back(make_record("s1", "anthropic", "claude-opus-4-7", 0.5));

    auto ins = aggregate("s1", records);
    ASSERT_EQ(ins.per_model_cost.size(), 2u);
    // Insertion order preserved.
    EXPECT_EQ(ins.per_model_cost[0].first,  "anthropic:claude-opus-4-7");
    EXPECT_DOUBLE_EQ(ins.per_model_cost[0].second, 1.5);
    EXPECT_EQ(ins.per_model_cost[1].first,  "openai:gpt-5");
    EXPECT_DOUBLE_EQ(ins.per_model_cost[1].second, 2.0);
}

TEST(AggregateTest, FiltersOutOtherSessions) {
    std::vector<UsageRecord> records;
    records.push_back(make_record("s1", "a", "m",  1.0));
    records.push_back(make_record("s2", "a", "m", 99.0));
    records.push_back(make_record("s1", "a", "m",  0.5));

    auto ins = aggregate("s1", records);
    EXPECT_EQ(ins.total_calls,         2u);
    EXPECT_DOUBLE_EQ(ins.total_cost_usd, 1.5);
}

TEST(AggregateTest, SessionIdPropagatesEvenWhenNoMatches) {
    std::vector<UsageRecord> records;
    records.push_back(make_record("s2", "a", "m", 10.0));

    auto ins = aggregate("does-not-exist", records);
    EXPECT_EQ(ins.session_id,    "does-not-exist");
    EXPECT_EQ(ins.total_calls,    0u);
    EXPECT_DOUBLE_EQ(ins.total_cost_usd, 0.0);
}

} // namespace
} // namespace euxis::metrics
