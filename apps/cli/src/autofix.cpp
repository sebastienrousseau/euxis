#include "euxis/cli/autofix.hpp"

#include <sstream>

namespace euxis::cli {

auto build_autofix_prompt(
    const std::string& finding,
    const std::string& file_path,
    const std::string& file_context,
    int line) -> std::string {

    std::ostringstream prompt;
    prompt << "You are a code fix assistant. Generate a minimal unified diff "
           << "to fix the following finding.\n\n"
           << "## Finding\n" << finding << "\n\n";
    if (!file_path.empty()) {
        prompt << "## File: " << file_path;
        if (line > 0) prompt << " (line " << line << ")";
        prompt << "\n";
    }
    if (!file_context.empty()) {
        prompt << "```\n" << file_context << "\n```\n\n";
    }
    prompt << "## Instructions\n"
           << "- Output ONLY a unified diff (--- a/ +++ b/ format)\n"
           << "- Keep the fix minimal — change only what's needed\n"
           << "- If you cannot generate a fix, output: NO_FIX_AVAILABLE\n";
    return prompt.str();
}

auto parse_autofix_response(
    const std::string& response,
    const std::string& finding,
    const std::string& file_path,
    const std::string& agent_id) -> AutofixSuggestion {

    AutofixSuggestion suggestion;
    suggestion.finding = finding;
    suggestion.file_path = file_path;
    suggestion.agent_id = agent_id;

    if (response.find("NO_FIX_AVAILABLE") != std::string::npos) {
        suggestion.explanation = "No automated fix available for this finding.";
        suggestion.confidence = 0.0;
        return suggestion;
    }

    // Extract unified diff block
    std::istringstream stream(response);
    std::string line;
    std::ostringstream diff;
    bool in_diff = false;
    std::ostringstream explanation;

    while (std::getline(stream, line)) {
        if (line.starts_with("--- ") || line.starts_with("diff ")) {
            in_diff = true;
        }
        if (in_diff) {
            diff << line << "\n";
            // End of diff hunk
            if (line.empty() && !diff.str().empty()) {
                // Check if next line continues the diff
            }
        } else {
            if (!line.empty() && line[0] != '#' && line[0] != '`') {
                explanation << line << " ";
            }
        }
    }

    suggestion.unified_diff = diff.str();
    suggestion.explanation = explanation.str();
    suggestion.confidence = suggestion.unified_diff.empty() ? 0.0 : 0.7;

    return suggestion;
}

auto format_autofix_suggestions(
    const std::vector<AutofixSuggestion>& suggestions) -> std::string {

    std::ostringstream out;
    int idx = 0;
    for (const auto& s : suggestions) {
        idx++;
        out << "--- Fix #" << idx << " (" << s.agent_id << ") ---\n";
        out << "Finding: " << s.finding << "\n";
        if (!s.file_path.empty()) {
            out << "File: " << s.file_path;
            if (s.line > 0) out << ":" << s.line;
            out << "\n";
        }
        if (!s.unified_diff.empty()) {
            out << "\n" << s.unified_diff;
        } else {
            out << "  " << s.explanation << "\n";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace euxis::cli
