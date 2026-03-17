#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
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

TEST_F(EvidenceFrameworkTest, CheckDecayDateOnlyFormat) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence(EvidenceGrade::Verified);
    // Use date-only format instead of full ISO-8601
    e.timestamp = "2020-01-01";
    // Old date, should be decayed
    EXPECT_TRUE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, CheckDecayInvalidTimestamp) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence(EvidenceGrade::Verified);
    e.timestamp = "not-a-date";
    // Unparseable timestamp should count as decayed
    EXPECT_TRUE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, CheckDecayRecentEvidence) {
    EvidenceFramework fw(tmp_);
    // Create evidence with a recent timestamp (today)
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);

    auto e = make_evidence(EvidenceGrade::Inferred);
    e.timestamp = buf;
    // Inferred has 30-day decay, so today's evidence should NOT be decayed
    EXPECT_FALSE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, CheckDecayObserved) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence(EvidenceGrade::Observed);
    e.timestamp = "2020-01-01T00:00:00";
    // Old observed evidence should be decayed (7-day window)
    EXPECT_TRUE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, CheckDecayMeasured) {
    EvidenceFramework fw(tmp_);
    auto e = make_evidence(EvidenceGrade::Measured);
    e.timestamp = "2020-01-01T00:00:00";
    // Old measured evidence should be decayed (1-day window)
    EXPECT_TRUE(fw.check_decay(e));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimLowConfidence) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 0.3;  // Below 0.5 threshold
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Verified));
    EXPECT_FALSE(fw.verify_claim(claim));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimExactlyThreshold) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 0.5;  // Exactly at threshold
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Verified));
    EXPECT_TRUE(fw.verify_claim(claim));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimE3Accepted) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 0.9;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Observed));
    EXPECT_TRUE(fw.verify_claim(claim));
}

TEST_F(EvidenceFrameworkTest, VerifyClaimE5Rejected) {
    EvidenceFramework fw(tmp_);
    Claim claim;
    claim.confidence = 1.0;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Speculated));
    EXPECT_FALSE(fw.verify_claim(claim));
}

TEST_F(EvidenceFrameworkTest, EvidenceFromJsonWithoutOptionals) {
    nlohmann::json j = {
        {"source_file", "test.cpp"},
        {"evidence_type", "test_result"},
        {"grade", "E3"},
        {"content", "Some content"},
        {"timestamp", "2026-01-01T00:00:00"},
    };
    // No source_line, no verification_cmd, no metadata
    auto e = Evidence::from_json(j);
    EXPECT_FALSE(e.source_line.has_value());
    EXPECT_FALSE(e.verification_cmd.has_value());
    EXPECT_EQ(e.grade, EvidenceGrade::Observed);
}

TEST_F(EvidenceFrameworkTest, EvidenceToJsonWithOptionals) {
    auto e = make_evidence();
    auto j = e.to_json();
    EXPECT_TRUE(j.contains("source_line"));
    EXPECT_EQ(j["source_line"], 42);
    EXPECT_TRUE(j.contains("verification_cmd"));
    EXPECT_EQ(j["verification_cmd"], "make test");
}

TEST_F(EvidenceFrameworkTest, EvidenceToJsonWithoutOptionals) {
    Evidence e;
    e.source_file = "test.cpp";
    e.evidence_type = "test";
    e.grade = EvidenceGrade::Verified;
    e.content = "content";
    e.timestamp = "2026-01-01";
    // source_line and verification_cmd are nullopt
    auto j = e.to_json();
    EXPECT_FALSE(j.contains("source_line"));
    EXPECT_FALSE(j.contains("verification_cmd"));
}

TEST_F(EvidenceFrameworkTest, LoadEvidenceFromCorruptLines) {
    EvidenceFramework fw(tmp_);
    // Store a valid one first
    auto h = fw.store_evidence(make_evidence());

    // Append a corrupt line to the DB
    auto db_path = tmp_ / "evidence.jsonl";
    std::ofstream f(db_path, std::ios::app);
    f << "not-valid-json\n";
    f << "\n";  // empty line
    f.close();

    // Should still find the valid evidence
    auto loaded = fw.load_evidence(h);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, "All tests pass");
}

TEST_F(EvidenceFrameworkTest, LoadAllEvidenceSkipsCorruptLines) {
    EvidenceFramework fw(tmp_);
    fw.store_evidence(make_evidence(EvidenceGrade::Verified));
    fw.store_evidence(make_evidence(EvidenceGrade::Measured));

    // Append corrupt lines
    auto db_path = tmp_ / "evidence.jsonl";
    std::ofstream f(db_path, std::ios::app);
    f << "corrupt\n";
    f << "\n";
    f.close();

    auto all = fw.load_all_evidence();
    EXPECT_EQ(all.size(), 2);
}

TEST_F(EvidenceFrameworkTest, LoadEvidenceNotFoundReturnsNullopt) {
    EvidenceFramework fw(tmp_);
    fw.store_evidence(make_evidence());
    auto loaded = fw.load_evidence("definitely-not-a-real-hash");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(EvidenceFrameworkTest, GradeFromStringAllValues) {
    EXPECT_EQ(grade_from_string("E2"), EvidenceGrade::Measured);
    EXPECT_EQ(grade_from_string("E3"), EvidenceGrade::Observed);
    EXPECT_EQ(grade_from_string("E4"), EvidenceGrade::Inferred);
}

TEST_F(EvidenceFrameworkTest, ClaimHighestGradeMultipleSame) {
    Claim claim;
    claim.statement = "Test";
    claim.confidence = 0.9;
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Measured));
    claim.supporting_evidence.push_back(make_evidence(EvidenceGrade::Measured));
    auto best = claim.highest_grade();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(*best, EvidenceGrade::Measured);
}

TEST_F(EvidenceFrameworkTest, ConstructorWithEmptyPath) {
    // Empty path triggers environment-based default
    EvidenceFramework fw({});
    EXPECT_FALSE(fw.evidence_dir().empty());
}

} // namespace
} // namespace euxis::metrics
