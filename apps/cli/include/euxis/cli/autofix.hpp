/// @file
/// @brief Autofix suggestion engine — generates unified diffs from findings.
#pragma once

#include <string>
#include <vector>

namespace euxis::cli {

/// A suggested fix for a finding.
struct AutofixSuggestion {
    std::string finding;           // The original finding text
    std::string file_path;         // File to modify
    int line{0};                   // Line number (0 = unknown)
    std::string unified_diff;      // Suggested fix as unified diff
    std::string explanation;       // Human-readable explanation
    std::string agent_id;          // Which agent generated this
    double confidence{0.0};        // 0.0-1.0 confidence in the fix
};

/// Build an autofix prompt for a given finding.
[[nodiscard]] auto build_autofix_prompt(
    const std::string& finding,
    const std::string& file_path,
    const std::string& file_context,
    int line = 0) -> std::string;

/// Parse an LLM response containing a unified diff suggestion.
[[nodiscard]] auto parse_autofix_response(
    const std::string& response,
    const std::string& finding,
    const std::string& file_path,
    const std::string& agent_id) -> AutofixSuggestion;

/// Format autofix suggestions for human-readable output.
[[nodiscard]] auto format_autofix_suggestions(
    const std::vector<AutofixSuggestion>& suggestions) -> std::string;

} // namespace euxis::cli
