#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "euxis/ensemble/deterministic.hpp"
#include "euxis/ensemble/runner.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::ensemble {
namespace {

using euxis::security::Confidence;
using euxis::security::Finding;
using euxis::security::Severity;

Finding make_finding(const std::string& rule_id = "test/rule",
                     Severity sev = Severity::Medium) {
    Finding f;
    f.rule_id   = rule_id;
    f.message   = "test message";
    f.severity  = sev;
    f.confidence = Confidence::Probable;
    f.primary_location.path = "f.cpp";
    f.primary_location.start_row    = 5;
    f.primary_location.start_column = 1;
    return f;
}

VerifierPtr always_true(std::string id = "always-true") {
    return std::make_shared<DeterministicVerifier>(std::move(id));
}

VerifierPtr always_false(std::string id = "always-false") {
    return std::make_shared<DeterministicVerifier>(
        std::move(id),
        [](const VoteRequest&) { return false; });
}

VerifierPtr matching(const std::string& rule_id_match,
                     std::string id = "matcher") {
    return std::make_shared<DeterministicVerifier>(
        std::move(id),
        [rule_id_match](const VoteRequest& r) {
            return r.rule_id == rule_id_match;
        });
}

TEST(Runner, EmptyVerifierSetLeavesFindingsUnchanged) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers;
    auto result = verify(std::move(findings), verifiers);
    EXPECT_EQ(result.stats.findings_verified, 1U);
    EXPECT_EQ(result.stats.verifiers_called,  0U);
    EXPECT_EQ(result.stats.confirmed,         0U);
    EXPECT_EQ(result.stats.rejected,          0U);
    ASSERT_EQ(result.findings.size(), 1U);
    EXPECT_TRUE(result.findings.front().ensemble_votes.empty());
}

TEST(Runner, AllVerifiersAgreeUpgradesToVerified) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("a"),
        always_true("b"),
        always_true("c"),
    };
    EnsembleConfig cfg;
    cfg.quorum = 2;
    auto result = verify(std::move(findings), verifiers, cfg);
    ASSERT_EQ(result.findings.size(), 1U);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Verified);
    EXPECT_EQ(result.findings.front().ensemble_votes.size(), 3U);
    EXPECT_EQ(result.stats.confirmed,   1U);
    EXPECT_EQ(result.stats.verifiers_called, 3U);
}

TEST(Runner, QuorumReachedNonUnanimousIsProbable) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("a"),
        always_true("b"),
        always_false("c"),
    };
    EnsembleConfig cfg;
    cfg.quorum = 2;
    auto result = verify(std::move(findings), verifiers, cfg);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Probable);
    EXPECT_EQ(result.stats.confirmed, 1U);
}

TEST(Runner, BelowQuorumWithSomeAgreementIsPossible) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("a"),
        always_false("b"),
        always_false("c"),
    };
    EnsembleConfig cfg;
    cfg.quorum = 2;
    auto result = verify(std::move(findings), verifiers, cfg);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Possible);
    EXPECT_EQ(result.stats.rejected, 1U);
    EXPECT_EQ(result.stats.confirmed, 0U);
}

TEST(Runner, AllVerifiersDisagreeDowngradesToUnknown) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_false("a"),
        always_false("b"),
        always_false("c"),
    };
    auto result = verify(std::move(findings), verifiers);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Unknown);
    EXPECT_EQ(result.stats.rejected, 1U);
}

TEST(Runner, RequireUnanimousRejectsNonUnanimousQuorum) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("a"),
        always_true("b"),
        always_false("c"),
    };
    EnsembleConfig cfg;
    cfg.quorum            = 2;
    cfg.require_unanimous = true;
    auto result = verify(std::move(findings), verifiers, cfg);
    // Quorum reached but not unanimous → rejected; confidence stays
    // Probable per the not-unanimous + quorum-reached path.
    EXPECT_EQ(result.stats.confirmed, 0U);
    EXPECT_EQ(result.stats.rejected,  1U);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Probable);
}

TEST(Runner, FindingsArePreservedInOrder) {
    std::vector<Finding> findings{
        make_finding("rule-1"),
        make_finding("rule-2"),
        make_finding("rule-3"),
    };
    std::vector<VerifierPtr> verifiers{always_true("only")};
    auto result = verify(std::move(findings), verifiers);
    ASSERT_EQ(result.findings.size(), 3U);
    EXPECT_EQ(result.findings[0].rule_id, "rule-1");
    EXPECT_EQ(result.findings[1].rule_id, "rule-2");
    EXPECT_EQ(result.findings[2].rule_id, "rule-3");
}

TEST(Runner, RuleSpecificVerifiersWorkAcrossFindings) {
    std::vector<Finding> findings{
        make_finding("alpha"),
        make_finding("beta"),
    };
    std::vector<VerifierPtr> verifiers{
        matching("alpha", "loves-alpha"),
        always_true("base-truth"),
    };
    EnsembleConfig cfg;
    cfg.quorum = 2;
    auto result = verify(std::move(findings), verifiers, cfg);
    ASSERT_EQ(result.findings.size(), 2U);
    EXPECT_EQ(result.findings[0].confidence, Confidence::Verified);
    EXPECT_EQ(result.findings[1].confidence, Confidence::Possible);
}

TEST(Runner, NullVerifierIsTolerated) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        nullptr,
        always_true("real"),
        nullptr,
    };
    EnsembleConfig cfg;
    cfg.quorum = 1;
    auto result = verify(std::move(findings), verifiers, cfg);
    EXPECT_EQ(result.stats.verifiers_called, 1U);
    EXPECT_EQ(result.findings.front().ensemble_votes.size(), 1U);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Verified);
}

TEST(Runner, MultipleVerifiersAttachVotesInCallOrder) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("first"),
        always_true("second"),
    };
    auto result = verify(std::move(findings), verifiers);
    ASSERT_EQ(result.findings.front().ensemble_votes.size(), 2U);
    EXPECT_EQ(result.findings.front().ensemble_votes[0].provider, "first");
    EXPECT_EQ(result.findings.front().ensemble_votes[1].provider, "second");
}

TEST(Runner, QuorumOfOneAcceptsSingleAgreement) {
    std::vector<Finding> findings{make_finding()};
    std::vector<VerifierPtr> verifiers{
        always_true("a"),
        always_false("b"),
    };
    EnsembleConfig cfg;
    cfg.quorum = 1;
    auto result = verify(std::move(findings), verifiers, cfg);
    EXPECT_EQ(result.stats.confirmed, 1U);
    EXPECT_EQ(result.findings.front().confidence, Confidence::Probable);
}

} // namespace
} // namespace euxis::ensemble
