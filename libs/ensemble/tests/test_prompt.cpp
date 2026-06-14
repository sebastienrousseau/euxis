#include <gtest/gtest.h>

#include "euxis/ensemble/prompt.hpp"

namespace euxis::ensemble {
namespace {

VoteRequest make_req() {
    VoteRequest r;
    r.rule_id     = "py.eval";
    r.message     = "Use of eval() is dangerous";
    r.source_path = "f.py";
    r.start_row    = 7;
    r.start_column = 5;
    r.severity    = euxis::security::Severity::High;
    r.snippet     = "eval(user_input)";
    return r;
}

TEST(Prompt, DefaultReviewerNameIsExperienced) {
    auto p = build_prompt(make_req());
    EXPECT_NE(p.find("experienced application-security reviewer"),
              std::string::npos);
}

TEST(Prompt, CustomReviewerNameIsHonored) {
    PromptConfig cfg;
    cfg.reviewer_name = "euxis security reviewer";
    auto p = build_prompt(make_req(), cfg);
    EXPECT_NE(p.find("euxis security reviewer"), std::string::npos);
}

TEST(Prompt, IncludesRuleSeverityMessageAndLocation) {
    auto p = build_prompt(make_req());
    EXPECT_NE(p.find("Rule: py.eval"),                     std::string::npos);
    EXPECT_NE(p.find("Severity: HIGH"),                    std::string::npos);
    EXPECT_NE(p.find("Use of eval() is dangerous"),        std::string::npos);
    EXPECT_NE(p.find("f.py:7:5"),                          std::string::npos);
}

TEST(Prompt, IncludesSnippetInCodeFence) {
    auto p = build_prompt(make_req());
    EXPECT_NE(p.find("```"),                std::string::npos);
    EXPECT_NE(p.find("eval(user_input)"),   std::string::npos);
}

TEST(Prompt, CropsSnippetToConfiguredLimit) {
    VoteRequest r = make_req();
    r.snippet = std::string(8 * 1024, 'A');
    PromptConfig cfg;
    cfg.max_snippet_bytes = 1024;
    auto p = build_prompt(r, cfg);
    EXPECT_LT(p.size(), 8 * 1024 + 1024);
}

TEST(Prompt, OmitsSnippetBlockWhenEmpty) {
    VoteRequest r = make_req();
    r.snippet.clear();
    auto p = build_prompt(r);
    EXPECT_EQ(p.find("Source snippet"), std::string::npos);
}

TEST(Prompt, AsksForStrictJsonShape) {
    auto p = build_prompt(make_req());
    EXPECT_NE(p.find("\"true_positive\""), std::string::npos);
    EXPECT_NE(p.find("\"confidence\""),    std::string::npos);
    EXPECT_NE(p.find("\"rationale\""),     std::string::npos);
}

} // namespace
} // namespace euxis::ensemble
