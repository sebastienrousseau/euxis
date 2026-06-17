#include <gtest/gtest.h>

#include "euxis/ensemble/deterministic.hpp"

namespace euxis::ensemble {
namespace {

TEST(Deterministic, DefaultPredicateAlwaysAgrees) {
    DeterministicVerifier v{"always-true"};
    VoteRequest req;
    req.rule_id = "test/anything";
    auto out = v.vote(req);
    EXPECT_TRUE(out.vote.true_positive);
    EXPECT_EQ(out.vote.provider, "always-true");
    EXPECT_FLOAT_EQ(out.vote.confidence, 1.0F);
    EXPECT_FALSE(out.rationale.empty());
}

TEST(Deterministic, PredicateDecidesVote) {
    DeterministicVerifier v{
        "rule-allowlist",
        [](const VoteRequest& r) { return r.rule_id == "good"; },
        0.95F, 0.10F,
        "matches allowlist", "not on allowlist",
    };
    VoteRequest hit;
    hit.rule_id = "good";
    auto out = v.vote(hit);
    EXPECT_TRUE(out.vote.true_positive);
    EXPECT_FLOAT_EQ(out.vote.confidence, 0.95F);
    EXPECT_EQ(out.rationale, "matches allowlist");

    VoteRequest miss;
    miss.rule_id = "other";
    auto out2 = v.vote(miss);
    EXPECT_FALSE(out2.vote.true_positive);
    EXPECT_FLOAT_EQ(out2.vote.confidence, 0.10F);
    EXPECT_EQ(out2.rationale, "not on allowlist");
}

TEST(Deterministic, ProviderIdMatchesConstruction) {
    DeterministicVerifier v{"my-id"};
    EXPECT_EQ(v.provider_id(), "my-id");
}

TEST(Deterministic, NullPredicateActsAsAlwaysTrue) {
    DeterministicVerifier v{"null-pred", nullptr};
    VoteRequest req;
    auto out = v.vote(req);
    // The implementation falls back to "always true" when the
    // function object is null, so the verifier is still well-
    // behaved instead of crashing.
    EXPECT_TRUE(out.vote.true_positive);
}

} // namespace
} // namespace euxis::ensemble
