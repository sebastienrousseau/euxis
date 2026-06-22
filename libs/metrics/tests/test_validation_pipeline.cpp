#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/metrics/validation_pipeline.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::metrics {
namespace {

class ValidationPipelineTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    std::unique_ptr<EvidenceFramework> fw_;
    std::unique_ptr<ValidationPipeline> pipeline_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_vp_test";
        std::filesystem::remove_all(tmp_);
        fw_ = std::make_unique<EvidenceFramework>(tmp_);
        pipeline_ = std::make_unique<ValidationPipeline>(fw_.get());
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }
};

TEST_F(ValidationPipelineTest, ExtractPercentageClaims) {
    auto claims = pipeline_->extract_quantitative_claims(
        "The success rate is 95.5% across all tests.");
    ASSERT_GE(claims.size(), 1);
    EXPECT_DOUBLE_EQ(*claims[0].value, 95.5);
}

TEST_F(ValidationPipelineTest, ExtractTimingClaims) {
    auto claims = pipeline_->extract_quantitative_claims(
        "Response time: 42ms under load.");
    ASSERT_GE(claims.size(), 1);
    EXPECT_DOUBLE_EQ(*claims[0].value, 42.0);
}

TEST_F(ValidationPipelineTest, ExtractSizeClaims) {
    auto claims = pipeline_->extract_quantitative_claims(
        "Memory usage peaked at 256MB.");
    ASSERT_GE(claims.size(), 1);
    EXPECT_DOUBLE_EQ(*claims[0].value, 256.0);
}

TEST_F(ValidationPipelineTest, NoClaimsInPlainText) {
    auto claims = pipeline_->extract_quantitative_claims(
        "The system works well and is stable.");
    EXPECT_TRUE(claims.empty());
}

TEST_F(ValidationPipelineTest, DetectForbiddenTerms) {
    auto terms = pipeline_->check_forbidden_terms(
        "The system is approximately 2x faster and probably stable.");
    ASSERT_GE(terms.size(), 2);
    // Should find "approximately" and "probably"
    bool found_approx = false, found_probably = false;
    for (const auto& t : terms) {
        if (t == "approximately") found_approx = true;
        if (t == "probably") found_probably = true;
    }
    EXPECT_TRUE(found_approx);
    EXPECT_TRUE(found_probably);
}

TEST_F(ValidationPipelineTest, NoForbiddenTermsCleanText) {
    auto terms = pipeline_->check_forbidden_terms(
        "Response time is 42ms. [E1: bench output]");
    EXPECT_TRUE(terms.empty());
}

TEST_F(ValidationPipelineTest, CitationCheckFindsEvidence) {
    std::string text = "Response time is 42ms. [E1: benchmark output]";
    auto claims = pipeline_->extract_quantitative_claims(text);
    auto result = pipeline_->check_citations(text, claims);
    EXPECT_EQ(result.cited_claims, result.total_claims);
}

TEST_F(ValidationPipelineTest, CitationCheckFindsMissing) {
    std::string text = "Response time is 42ms with no citation.";
    auto claims = pipeline_->extract_quantitative_claims(text);
    auto result = pipeline_->check_citations(text, claims);
    EXPECT_GT(result.uncited_claims.size(), 0);
}

TEST_F(ValidationPipelineTest, ValidateReportPasses) {
    std::string text = "Success rate: 99% [E1: test output]. "
                       "Latency: 5ms [E2: Measured in benchmark].";
    auto result = pipeline_->validate_report(text);
    EXPECT_TRUE(result.passed);
    EXPECT_GE(result.total_claims, 2);
}

TEST_F(ValidationPipelineTest, ValidateReportFailsForbiddenTerms) {
    std::string text = "Success rate is approximately 95%. "
                       "[E1: probably from test output]";
    auto result = pipeline_->validate_report(text);
    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.issues.empty());
}

TEST_F(ValidationPipelineTest, ValidateReportFailsUncited) {
    // Low pass threshold to isolate uncited vs forbidden
    auto strict = ValidationPipeline(fw_.get(), 1.0);
    std::string text = "Latency is 10ms. Memory is 128MB.";
    auto result = strict.validate_report(text);
    EXPECT_FALSE(result.passed);
    EXPECT_LT(result.pass_rate, 1.0);
}

TEST_F(ValidationPipelineTest, CheckCitationsWithNoCitations) {
    // Claims that have no citations at all (covers line 79 — uncited path)
    std::string text = "Throughput is 500MB and latency is 10ms with no evidence.";
    auto claims = pipeline_->extract_quantitative_claims(text);
    ASSERT_GE(claims.size(), 1);
    auto result = pipeline_->check_citations(text, claims);
    EXPECT_EQ(result.cited_claims, 0);
    EXPECT_EQ(result.uncited_claims.size(), static_cast<size_t>(result.total_claims));
    // check_forbidden_terms should also be called (covers line 105)
    // This text has no forbidden terms, so the vector should be empty
    EXPECT_TRUE(result.forbidden_terms_found.empty());
}

TEST_F(ValidationPipelineTest, CheckForbiddenTermsMultiple) {
    // Exercise check_forbidden_terms directly with multiple hits (covers line 105)
    auto terms = pipeline_->check_forbidden_terms(
        "I think it seems likely that maybe the results are around 50%.");
    // Should find: "seems", "likely", "I think", "maybe", "around"
    EXPECT_GE(terms.size(), 4);
}

TEST_F(ValidationPipelineTest, CheckCitationsWithForbiddenTerms) {
    // Combines citations check with forbidden terms (covers lines 79+105 together)
    std::string text = "Response time is approximately 42ms. Perhaps memory is 256MB.";
    auto claims = pipeline_->extract_quantitative_claims(text);
    auto result = pipeline_->check_citations(text, claims);
    // Should find forbidden terms: "approximately", "perhaps"
    EXPECT_GE(result.forbidden_terms_found.size(), 1);
}

} // namespace
} // namespace euxis::metrics

// NOLINTEND(bugprone-unchecked-optional-access)
