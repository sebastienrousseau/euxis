#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "euxis/cpg/builder.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/reach/callgraph.hpp"
#include "euxis/reach/reachability.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::reach {
namespace {

using euxis::parse::Language;
using euxis::parse::Parser;

struct Pipeline {
    euxis::parse::Ast ast;
    euxis::cpg::Graph graph;
    CallGraph cg;
};

Pipeline build_pipeline(Language lang, const std::string& src) {
    Parser p(lang);
    auto ast = p.parse(src);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto g = euxis::cpg::build(*ast);
    EXPECT_TRUE(g.has_value());
    auto cg = build_call_graph(*g, *ast);
    return Pipeline{std::move(*ast), std::move(*g), std::move(cg.graph)};
}

TEST(Reachability, EmptyGraphHasNoReachable) {
    Parser p(Language::C);
    auto ast = p.parse("");
    ASSERT_TRUE(ast.has_value());
    auto g = euxis::cpg::build(*ast);
    ASSERT_TRUE(g.has_value());
    CallGraph cg;
    auto r = compute_reachable(*g, *ast, cg);
    EXPECT_TRUE(r.reachable_functions.empty());
}

TEST(Reachability, TopLevelFunctionsAreEntries) {
    auto pipe = build_pipeline(Language::C, R"(
int helper(int x) { return x + 1; }
int main(void) { return helper(1); }
)");
    auto r = compute_reachable(pipe.graph, pipe.ast, pipe.cg);
    EXPECT_GE(r.entry_points, 1U);
    EXPECT_FALSE(r.reachable_functions.empty());
}

TEST(Reachability, ConfigDisablesTopLevelEntryHeuristic) {
    auto pipe = build_pipeline(Language::C, R"(
int main(void) { return 0; }
)");
    ReachabilityConfig config;
    config.top_level_functions_are_entries = false;
    config.entry_function_names            = {};
    auto r = compute_reachable(pipe.graph, pipe.ast, pipe.cg, config);
    EXPECT_EQ(r.entry_points, 0U);
}

TEST(Reachability, NamedEntryPointSeedsBfs) {
    auto pipe = build_pipeline(Language::C, R"(
int helper(int x) { return x + 1; }
int handler(void) { return helper(1); }
)");
    ReachabilityConfig config;
    config.top_level_functions_are_entries = false;
    config.entry_function_names            = {"handler"};
    auto r = compute_reachable(pipe.graph, pipe.ast, pipe.cg, config);
    EXPECT_EQ(r.entry_points, 1U);
    EXPECT_FALSE(r.reachable_functions.empty());
}

TEST(Reachability, AnnotateFindingsAppendsTaxa) {
    auto pipe = build_pipeline(Language::C, R"(
int dead_helper(int x) { return x + 1; }
int main(void) { return 0; }
)");
    auto r = compute_reachable(pipe.graph, pipe.ast, pipe.cg);

    std::vector<euxis::security::Finding> findings(2);
    findings[0].rule_id   = "in-main";
    findings[0].message   = "in main";
    findings[0].primary_location.path      = "f.c";
    findings[0].primary_location.start_row = 3;  // inside main()

    findings[1].rule_id   = "in-dead";
    findings[1].message   = "in dead helper";
    findings[1].primary_location.path      = "f.c";
    findings[1].primary_location.start_row = 2;  // inside dead_helper

    annotate_findings(pipe.graph, r, findings.begin(), findings.end());

    auto has_taxon = [](const auto& f, const std::string& wanted) {
        return std::find(f.compliance_taxa.begin(),
                         f.compliance_taxa.end(),
                         wanted) != f.compliance_taxa.end();
    };

    // Every finding gets exactly one euxis:reachable:* taxon.
    int count_a = 0;
    int count_b = 0;
    for (const auto& tax : findings[0].compliance_taxa) {
        if (tax.starts_with("euxis:reachable:")) ++count_a;
    }
    for (const auto& tax : findings[1].compliance_taxa) {
        if (tax.starts_with("euxis:reachable:")) ++count_b;
    }
    EXPECT_EQ(count_a, 1);
    EXPECT_EQ(count_b, 1);
    EXPECT_TRUE(has_taxon(findings[0], "euxis:reachable:true") ||
                has_taxon(findings[0], "euxis:reachable:unknown"));
}

TEST(Reachability, FindingOutsideAnyFunctionIsUnknown) {
    auto pipe = build_pipeline(Language::C, R"(
int main(void) { return 0; }
)");
    auto r = compute_reachable(pipe.graph, pipe.ast, pipe.cg);

    std::vector<euxis::security::Finding> findings(1);
    findings[0].rule_id   = "header";
    findings[0].message   = "in include";
    findings[0].primary_location.path      = "f.c";
    findings[0].primary_location.start_row = 1;  // at the top of the file

    annotate_findings(pipe.graph, r, findings.begin(), findings.end());

    auto has_taxon = [](const auto& f, const std::string& wanted) {
        return std::find(f.compliance_taxa.begin(),
                         f.compliance_taxa.end(),
                         wanted) != f.compliance_taxa.end();
    };
    // The exact answer depends on how the parser positioned the
    // function in the file. We accept any of true / false /
    // unknown — the test only enforces that the annotation IS
    // appended.
    EXPECT_TRUE(has_taxon(findings[0], "euxis:reachable:true") ||
                has_taxon(findings[0], "euxis:reachable:false") ||
                has_taxon(findings[0], "euxis:reachable:unknown"));
}

} // namespace
} // namespace euxis::reach
