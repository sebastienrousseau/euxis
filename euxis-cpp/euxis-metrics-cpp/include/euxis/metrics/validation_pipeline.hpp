#pragma once

#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "evidence.hpp"

namespace euxis::metrics {

struct ExtractedClaim {
    std::string matched_text;
    double value{0.0};
    std::string unit;
    std::string context;
    int position_start{0};
    int position_end{0};
};

struct CitationCheckResult {
    int total_claims{0};
    int cited_claims{0};
    std::vector<std::string> uncited_claims;
    std::vector<std::string> forbidden_terms_found;
};

struct ValidationResult {
    bool passed{false};
    int total_claims{0};
    int verified_claims{0};
    double pass_rate{0.0};
    CitationCheckResult citation_check;
    std::vector<std::string> issues;
};

class ValidationPipeline {
public:
    explicit ValidationPipeline(
        EvidenceFramework* framework = nullptr,
        double pass_threshold = 0.8);

    auto extract_quantitative_claims(const std::string& text)
        -> std::vector<ExtractedClaim>;

    auto check_citations(const std::string& text,
                         const std::vector<ExtractedClaim>& claims)
        -> CitationCheckResult;

    auto check_forbidden_terms(const std::string& text)
        -> std::vector<std::string>;

    auto validate_report(const std::string& text) -> ValidationResult;

private:
    EvidenceFramework* framework_;
    double pass_threshold_;
    std::vector<std::string> quantitative_patterns_;
    std::vector<std::string> citation_patterns_;
    std::vector<std::string> forbidden_terms_;

    void init_patterns();
};

} // namespace euxis::metrics
