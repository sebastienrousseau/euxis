#include "euxis/metrics/validation_pipeline.hpp"

#include <algorithm>
#include <regex>

namespace euxis::metrics {

ValidationPipeline::ValidationPipeline(EvidenceFramework* framework,
                                         double pass_threshold)
    : framework_(framework), pass_threshold_(pass_threshold) {
    init_patterns();
}

void ValidationPipeline::init_patterns() {
    quantitative_patterns_ = {
        R"(\b\d+\.?\d*%)",
        R"(\b\d+\.?\d*\s*(?:ms|milliseconds?|seconds?|minutes?|hours?)\b)",
        R"(\b\d+\.?\d*\s*(?:MB|GB|KB|bytes?)\b)",
        R"(\b\d+\.?\d*x\s+(?:faster|slower|more|less|better|worse)\b)",
        R"(\b(?:average|median|mean|max|min|total|sum)\s*[:\-]?\s*\d+\.?\d*\b)",
        R"(\b(?:success|failure|error)\s+rate\s*[:\-]?\s*\d+\.?\d*%?\b)",
    };

    citation_patterns_ = {
        R"(\[E[1-4]: .+\])",
        R"((?:Observed|Measured|Verified|Inferred) (?:at|in|by) .+)",
    };

    forbidden_terms_ = {
        "approximately", "roughly", "around",
        "seems", "appears", "likely", "probably",
        "I think", "maybe", "perhaps",
    };
}

auto ValidationPipeline::extract_quantitative_claims(const std::string& text)
    -> std::vector<ExtractedClaim> {
    std::vector<ExtractedClaim> claims;

    for (const auto& pattern_str : quantitative_patterns_) {
        std::regex pattern(pattern_str, std::regex::icase);
        auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            auto& match = *it;
            ExtractedClaim claim;
            claim.matched_text = match.str();
            claim.position_start = static_cast<int>(match.position());
            claim.position_end = claim.position_start +
                                 static_cast<int>(match.length());

            // Extract context (100 chars around match)
            int ctx_start = std::max(0, claim.position_start - 100);
            int ctx_end = std::min(static_cast<int>(text.size()),
                                   claim.position_end + 100);
            claim.context = text.substr(
                static_cast<size_t>(ctx_start),
                static_cast<size_t>(ctx_end - ctx_start));

            // Extract numeric value
            std::smatch num_match;
            std::string mt = claim.matched_text;
            if (std::regex_search(mt, num_match, std::regex(R"(\d+\.?\d*)"))) {
                claim.value = std::stod(num_match.str());
            }

            // Extract unit
            std::smatch unit_match;
            if (std::regex_search(mt, unit_match, std::regex(R"([a-zA-Z%]+)"))) {
                claim.unit = unit_match.str();
            }

            claims.push_back(std::move(claim));
        }
    }

    return claims;
}

auto ValidationPipeline::check_citations(
    const std::string& text,
    const std::vector<ExtractedClaim>& claims) -> CitationCheckResult {
    CitationCheckResult result;
    result.total_claims = static_cast<int>(claims.size());

    for (const auto& claim : claims) {
        bool cited = false;
        for (const auto& pattern_str : citation_patterns_) {
            std::regex pattern(pattern_str, std::regex::icase);
            if (std::regex_search(claim.context, pattern)) {
                cited = true;
                break;
            }
        }
        if (cited) {
            result.cited_claims++;
        } else {
            result.uncited_claims.push_back(claim.matched_text);
        }
    }

    result.forbidden_terms_found = check_forbidden_terms(text);
    return result;
}

auto ValidationPipeline::check_forbidden_terms(const std::string& text)
    -> std::vector<std::string> {
    std::vector<std::string> found;
    auto lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& term : forbidden_terms_) {
        auto term_lower = term;
        std::transform(term_lower.begin(), term_lower.end(),
                       term_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find(term_lower) != std::string::npos) {
            found.push_back(term);
        }
    }
    return found;
}

auto ValidationPipeline::validate_report(const std::string& text)
    -> ValidationResult {
    ValidationResult result;

    auto claims = extract_quantitative_claims(text);
    result.total_claims = static_cast<int>(claims.size());

    result.citation_check = check_citations(text, claims);

    // Count verified claims (those with proper citations)
    result.verified_claims = result.citation_check.cited_claims;

    if (result.total_claims > 0) {
        result.pass_rate = static_cast<double>(result.verified_claims) /
                           static_cast<double>(result.total_claims);
    }

    // Check for forbidden terms
    if (!result.citation_check.forbidden_terms_found.empty()) {
        for (const auto& term : result.citation_check.forbidden_terms_found) {
            result.issues.push_back("Forbidden term: " + term);
        }
    }

    // Check for uncited claims
    for (const auto& uncited : result.citation_check.uncited_claims) {
        result.issues.push_back("Uncited claim: " + uncited);
    }

    result.passed = result.pass_rate >= pass_threshold_ &&
                    result.citation_check.forbidden_terms_found.empty();
    return result;
}

} // namespace euxis::metrics
