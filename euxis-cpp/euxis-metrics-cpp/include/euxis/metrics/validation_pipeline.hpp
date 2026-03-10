/// @file
/// @brief Pipeline for validating generated reports against evidentiary standards.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "evidence.hpp"

namespace euxis::metrics {

/// @brief A potential claim extracted from raw text.
struct ExtractedClaim {
    std::string matched_text;
    int position_start;
    int position_end;
    std::string context;
    std::optional<double> value;
    std::optional<std::string> unit;
};

/// @brief Result of a citation and forbidden term scan.
struct CitationCheckResult {
    int total_claims{0};
    int cited_claims{0};
    std::vector<std::string> uncited_claims;
    std::vector<std::string> forbidden_terms_found;
};

/// @brief Unified validation result for a report.
struct ValidationResult {
    bool passed{false};
    int total_claims{0};
    int verified_claims{0};
    double pass_rate{0.0};
    CitationCheckResult citation_check;
    std::vector<std::string> issues;
};

/// @brief Orchestrates the extraction and verification of claims within a text body.
class ValidationPipeline {
public:
    /// @brief Construct pipeline with an optional evidence framework and pass threshold.
    explicit ValidationPipeline(EvidenceFramework* framework = nullptr,
                                 double pass_threshold = 0.8);

    /// @brief run full validation on a report text.
    auto validate_report(const std::string& text) -> ValidationResult;

    /// @brief Identify quantitative statements in text.
    auto extract_quantitative_claims(const std::string& text)
        -> std::vector<ExtractedClaim>;
    
    /// @brief Verify that extracted claims carry proper citations.
    auto check_citations(const std::string& text,
                         const std::vector<ExtractedClaim>& claims)
        -> CitationCheckResult;
    
    /// @brief scan for prohibited or biased terminology.
    auto check_forbidden_terms(const std::string& text)
        -> std::vector<std::string>;

private:
    EvidenceFramework* framework_;
    double pass_threshold_;

    std::vector<std::string> quantitative_patterns_;
    std::vector<std::string> citation_patterns_;
    std::vector<std::string> forbidden_terms_;

    void init_patterns();
};

} // namespace euxis::metrics
