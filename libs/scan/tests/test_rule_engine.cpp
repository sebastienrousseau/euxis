#include <gtest/gtest.h>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/graph.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/scan/rule_engine.hpp"
#include "euxis/scan/rule_loader.hpp"

namespace euxis::scan {
namespace {

using euxis::parse::Language;
using euxis::parse::Parser;

struct Pipeline {
    euxis::parse::Ast ast;
    euxis::cpg::Graph graph;
};

Pipeline build_pipeline(Language lang, const std::string& src) {
    Parser p(lang);
    auto ast = p.parse(src);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto g = euxis::cpg::build(*ast);
    EXPECT_TRUE(g.has_value());
    return Pipeline{std::move(*ast), std::move(*g)};
}

TEST(Engine, EmitsFindingForLiteralMatch) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: eval-call
    message: "Use of eval() is dangerous"
    severity: ERROR
    languages: [javascript]
    pattern: eval
)").value();

    auto pipe = build_pipeline(Language::JavaScript,
        "const x = eval('1+1');");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "file.js");
    EXPECT_GT(result.stats.findings_emitted, 0U);
    EXPECT_FALSE(result.findings.empty());
    EXPECT_EQ(result.findings.front().rule_id,
              "opengrep/anonymous/eval-call");
    EXPECT_EQ(result.findings.front().severity,
              euxis::security::Severity::High);
}

TEST(Engine, LanguageMismatchIsSkipped) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: rust-only
    message: "rust-only rule"
    severity: WARNING
    languages: [rust]
    pattern: unsafe
)").value();

    auto pipe = build_pipeline(Language::JavaScript,
        "var unsafe = 1;");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "file.js");
    EXPECT_GT(result.stats.skipped_language_mismatch, 0U);
    EXPECT_EQ(result.stats.findings_emitted, 0U);
}

TEST(Engine, NoLanguageMeansUniversalApplication) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: generic-secret
    message: "Possible secret"
    severity: WARNING
    pattern: AWS_ACCESS_KEY_ID
)").value();

    auto pipe = build_pipeline(Language::Python,
        "AWS_ACCESS_KEY_ID = 'AKIA...'");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "file.py");
    EXPECT_GT(result.stats.findings_emitted, 0U);
}

TEST(Engine, CompositePatternRequiresAllChildren) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: pickle-loads
    message: "Insecure deserialization"
    severity: ERROR
    languages: [python]
    patterns:
      - pattern: pickle
      - pattern: loads
)").value();

    // Both substrings present → at least one match.
    auto pipe1 = build_pipeline(Language::Python,
        "import pickle\ndata = pickle.loads(payload)");
    auto r1 = apply_rules(pack, pipe1.ast, pipe1.graph, "ok.py");
    EXPECT_GT(r1.stats.findings_emitted, 0U);

    // Only one substring present → no match.
    auto pipe2 = build_pipeline(Language::Python,
        "import pickle\ndata = pickle.dumps(payload)");
    auto r2 = apply_rules(pack, pipe2.ast, pipe2.graph, "miss.py");
    EXPECT_EQ(r2.stats.findings_emitted, 0U);
}

TEST(Engine, AlternationMatchesAnyChild) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: weak-hash
    message: "Weak hash"
    severity: WARNING
    languages: [python]
    pattern-either:
      - pattern: md5
      - pattern: sha1
)").value();

    auto pipe1 = build_pipeline(Language::Python,
        "import hashlib\nh = hashlib.md5()");
    auto r1 = apply_rules(pack, pipe1.ast, pipe1.graph, "a.py");
    EXPECT_GT(r1.stats.findings_emitted, 0U);

    auto pipe2 = build_pipeline(Language::Python,
        "import hashlib\nh = hashlib.sha1()");
    auto r2 = apply_rules(pack, pipe2.ast, pipe2.graph, "b.py");
    EXPECT_GT(r2.stats.findings_emitted, 0U);

    auto pipe3 = build_pipeline(Language::Python,
        "import hashlib\nh = hashlib.sha256()");
    auto r3 = apply_rules(pack, pipe3.ast, pipe3.graph, "c.py");
    EXPECT_EQ(r3.stats.findings_emitted, 0U);
}

TEST(Engine, MetadataPropagatesIntoFinding) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: insecure-eval
    message: "eval"
    severity: ERROR
    languages: [javascript]
    pattern: eval
    metadata:
      cwe: "CWE-94"
      owasp: "A03:2025"
      references:
        - https://owasp.org/A03
)").value();

    auto pipe = build_pipeline(Language::JavaScript, "var x = eval(s);");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "x.js");
    ASSERT_FALSE(result.findings.empty());
    const auto& f = result.findings.front();
    ASSERT_TRUE(f.cwe.has_value());
    EXPECT_EQ(f.cwe->id, "CWE-94");
    EXPECT_EQ(f.owasp,
              euxis::security::OwaspCategory::A03_SupplyChainFailures);
    ASSERT_EQ(f.compliance_taxa.size(), 1U);
}

TEST(Engine, FindingsCarryLocationFromMatchedNode) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: literal-x
    message: "literal x"
    severity: INFO
    languages: [c]
    pattern: x
)").value();

    auto pipe = build_pipeline(Language::C,
        "int main(void) { int x = 1; return x; }");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "file.c");
    ASSERT_FALSE(result.findings.empty());
    EXPECT_EQ(result.findings.front().primary_location.path, "file.c");
    // Locations are 1-based on output (we converted from
    // tree-sitter's 0-based row/column).
    EXPECT_GE(result.findings.front().primary_location.start_line, 1);
}

TEST(Engine, EmptyPackProducesNoFindings) {
    RulePack pack;
    pack.name = "empty";
    auto pipe = build_pipeline(Language::C, "int main(void){return 0;}");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "file.c");
    EXPECT_EQ(result.stats.findings_emitted, 0U);
    EXPECT_EQ(result.stats.rules_evaluated, 0U);
}

TEST(Engine, FingerprintIsStableAcrossRuns) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: lit
    message: "lit"
    severity: INFO
    languages: [c]
    pattern: literal_marker_xyz
)").value();

    auto pipe1 = build_pipeline(Language::C,
        "/* literal_marker_xyz */ int main(void){return 0;}");
    auto r1 = apply_rules(pack, pipe1.ast, pipe1.graph, "stable.c");
    ASSERT_FALSE(r1.findings.empty());

    auto pipe2 = build_pipeline(Language::C,
        "/* literal_marker_xyz */ int main(void){return 0;}");
    auto r2 = apply_rules(pack, pipe2.ast, pipe2.graph, "stable.c");
    ASSERT_FALSE(r2.findings.empty());

    EXPECT_EQ(r1.findings.front().stable_fingerprint,
              r2.findings.front().stable_fingerprint);
    EXPECT_EQ(r1.findings.front().stable_fingerprint.size(), 32U);
}

TEST(Engine, StatsReportRulesEvaluated) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: a
    message: "a"
    severity: INFO
    pattern: never_match
  - id: b
    message: "b"
    severity: INFO
    pattern: also_never_match
)").value();

    auto pipe = build_pipeline(Language::Python, "x = 1");
    auto result = apply_rules(pack, pipe.ast, pipe.graph, "x.py");
    EXPECT_EQ(result.stats.rules_evaluated, 2U);
    EXPECT_EQ(result.stats.findings_emitted, 0U);
}

} // namespace
} // namespace euxis::scan
