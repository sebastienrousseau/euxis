#include "euxis/ensemble/prompt.hpp"

#include <sstream>
#include <string>
#include <string_view>

#include "euxis/security/finding.hpp"

namespace euxis::ensemble {

namespace {

std::string_view crop(std::string_view src, std::size_t max_bytes) {
    if (src.size() <= max_bytes) return src;
    return src.substr(0, max_bytes);
}

} // namespace

auto build_prompt(const VoteRequest& req, const PromptConfig& cfg)
    -> std::string {
    std::ostringstream os;

    if (cfg.reviewer_name.empty()) {
        os << "You are an experienced application-security reviewer.";
    } else {
        os << "You are " << cfg.reviewer_name << ".";
    }
    os << "\n\n"
       << "A static analysis tool produced the following finding. "
          "Decide whether it is a true positive — that is, an "
          "actual security defect a human reviewer would flag — "
          "or a false positive that should be suppressed.\n\n";

    os << "Rule: "     << req.rule_id  << "\n"
       << "Severity: " << euxis::security::severity_label(req.severity) << "\n"
       << "Message: "  << req.message  << "\n"
       << "File: "     << req.source_path
       << ":"           << req.start_row
       << ":"           << req.start_column << "\n\n";

    if (!req.snippet.empty()) {
        os << "Source snippet:\n```\n"
           << crop(req.snippet, cfg.max_snippet_bytes)
           << "\n```\n\n";
    }

    os << "Reply with a single JSON object on one line, no prose, "
          "no Markdown. The object must have exactly three fields:\n"
          "  - \"true_positive\": boolean\n"
          "  - \"confidence\":    number in [0.0, 1.0]\n"
          "  - \"rationale\":     short string, 200 chars or fewer\n"
          "\n"
          "Example: "
          "{\"true_positive\": true, \"confidence\": 0.92, "
          "\"rationale\": \"untrusted input flows to eval\"}\n";

    return os.str();
}

} // namespace euxis::ensemble
