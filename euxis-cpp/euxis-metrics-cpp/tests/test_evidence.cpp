#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/metrics/evidence.hpp"

namespace euxis::metrics {
namespace {

class EvidenceFrameworkTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_evidence_test";
        std::filesystem::remove_all(tmp_);
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }

    static auto make_evidence(EvidenceGrade grade = EvidenceGrade::Verified)
        -> Evidence {
        return Evidence{
            .source_file = "test.cpp",
            .source_line = 42,
            .evidence_type = "test_result",
            .grade = grade,
            .content = "All tests pass",
            .timestamp = "2026-03-01T12:00:00",
            .verification_cmd = "make test",
            .metadata = {{"suite", "unit"}},
        };
    }
};

TEST_F(EvidenceFrameworkTest, GradeRoundTrip) {
    EXPECT_EQ(grade_to_string(EvidenceGrade::Verified), "E1");
    EXPECT_EQ(grade_to_string(EvidenceGrade::Measured), "E2");
    EXPECT_EQ(grade_to_string(EvidenceGrade::Observed), "E3");
    EXPECT_EQ(grade_to_string(EvidenceGrade::Inferred), "E4");
    EXPECT_EQ(grade_to_string(EvidenceGrade::Speculated), "E5");

    EXPECT_EQ(grade_from_string("E1"), EvidenceGrade::Verified);
    EXPECT_EQ(grade_from_string("E5"), EvidenceGrade::Speculated);
    EXPECT_EQ(grade_from_string("junk"), EvidenceGrade::Speculated);
}

TEST_F(EvidenceFrameworkTest, EvidenceJsonRoundTrip) {
    auto e = make_evidence();
    auto j = e.to_json();
    auto e2 = Evidence::from_json(j);
    EXPECT_EQ(e2.source_file, "test.cpp");
    EXPECT_EQ(e2.source_line, 42);
    EXPECT_EQ(e2.grade, EvidenceGrade::Verified);
    EXPECT_EQ(e2.content, "All tests pass");
    EXPECT_EQ(e2.verification_cmd, "make test");
}

TEST_F(EvidenceFrameworkTest, HashDeterministic) {
    auto e = make_evidence();
    EXPECT_EQ(e.hash(), e.hash());
    EXPECT_FALSE(e.hash().empty());
    EXPECT_EQ(e.hash().size(), 12);
}

TEST_F(EvidenceFrameworkTest, StoreAndLoad) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence();
    auto h = fw.store_evidence(e);
    EXPECT_FALSE(h.empty());

    auto loaded = fw.load_evidence(h);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, "All tests pass");
}

TEST_F(EvidenceFrameworkTest, LoadMissing) {
    EvidenceFramework fw(tmp_);
    auto loaded = fw.load_evidence("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(EvidenceFrameworkTest, LoadAll) {
    EvidenceFramework fw(tmp_);
    fw.store_evidence(make_evidence(EvidenceGrade::Verified));
    fw.store_evidence(make_evidence(EvidenceGrade::Measured));
    auto all = fw.load_all_evidence();
    EXPECT_EQ(all.size(), 2);
}

TEST_F(EvidenceFrameworkTest, SpeculatedAlwaysDecayed) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence(EvidenceGrade::Speculated);
    EXPECT_TRUE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, ClaimHighestGrade) {
    Claim claim;
    claim.statement = "Test passes";
    claim.confidence = 0.9;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Observed));
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Verified));
    auto best = claim.highest_grade();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(*best, EvidenceGrade::Verified);
}

TEST_F(EvidenceFrameworkTest, ClaimNoEvidence) {
    Claim claim;
    claim.statement = "Unsubstantiated";
    EXPECT_FALSE(claim.highest_grade().has_value());
}

TEST_F(EvidenceFrameworkTest, VerifyClaimRequiresEvidence) {
    EvidenceFramework fw(tmp_);
    Claim empty_claim;
    empty_claim.confidence = 1.0;
    EXPECT_FALSE(fw.verify_claim(empty_claim));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimRejectsE4Only) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 0.9;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Inferred));
    EXPECT_FALSE(fw.verify_claim(claim));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimAcceptsE1) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 0.9;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Verified));
    EXPECT_TRUE(fw.verify_claim(claim));
}

} // namespace
} // namespace euxis::metrics
