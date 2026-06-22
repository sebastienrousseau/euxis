#include <gtest/gtest.h>

#include "euxis/scan/rule.hpp"
#include "euxis/scan/rule_loader.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::scan {
namespace {

TEST(Loader, MinimalSingleRule) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: dangerous-eval
    message: "Use of eval() is dangerous"
    severity: ERROR
    languages: [javascript]
    pattern: eval
)");
    ASSERT_TRUE(pack.has_value()) << (pack ? "" : pack.error().message);
    ASSERT_EQ(pack->rules.size(), 1U);
    const auto& r = pack->rules[0];
    EXPECT_EQ(r.id,       "dangerous-eval");
    EXPECT_EQ(r.message,  "Use of eval() is dangerous");
    EXPECT_EQ(r.severity, RuleSeverity::Error);
    ASSERT_EQ(r.languages.size(), 1U);
    EXPECT_EQ(r.languages[0], euxis::parse::Language::JavaScript);
    EXPECT_EQ(r.pattern.kind, PatternKind::Literal);
    EXPECT_EQ(r.pattern.text, "eval");
}

TEST(Loader, CompositePattern) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: insecure-deserialization
    message: "Insecure deserialization"
    severity: ERROR
    languages: [python]
    patterns:
      - pattern: pickle
      - pattern: loads
)");
    ASSERT_TRUE(pack.has_value());
    ASSERT_EQ(pack->rules.size(), 1U);
    EXPECT_EQ(pack->rules[0].pattern.kind,           PatternKind::Composite);
    ASSERT_EQ(pack->rules[0].pattern.children.size(), 2U);
    EXPECT_EQ(pack->rules[0].pattern.children[0].text, "pickle");
    EXPECT_EQ(pack->rules[0].pattern.children[1].text, "loads");
}

TEST(Loader, AlternationPattern) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: weak-hash
    message: "Weak cryptographic hash"
    severity: WARNING
    languages: [python]
    pattern-either:
      - pattern: md5
      - pattern: sha1
)");
    ASSERT_TRUE(pack.has_value());
    EXPECT_EQ(pack->rules[0].pattern.kind, PatternKind::Alternation);
    ASSERT_EQ(pack->rules[0].pattern.children.size(), 2U);
}

TEST(Loader, MetadataExtractsCweAndOwasp) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: xss
    message: "XSS"
    severity: ERROR
    languages: [javascript]
    pattern: innerHTML
    metadata:
      cwe: "CWE-79"
      owasp: "A03:2025"
      references:
        - https://owasp.org/Top10/2025/A03_2025-Software_Supply_Chain_Failures/
)");
    ASSERT_TRUE(pack.has_value());
    const auto& r = pack->rules[0];
    ASSERT_TRUE(r.metadata.primary_cwe.has_value());
    EXPECT_EQ(*r.metadata.primary_cwe, "CWE-79");
    ASSERT_TRUE(r.metadata.primary_owasp.has_value());
    EXPECT_EQ(*r.metadata.primary_owasp,
              euxis::security::OwaspCategory::A03_SupplyChainFailures);
    ASSERT_EQ(r.metadata.references.size(), 1U);
}

TEST(Loader, MetadataListsAreReduced) {
    // CWE and OWASP both surface as a list head when the YAML uses a sequence.
    auto pack = load_rules_yaml(R"(
rules:
  - id: x
    message: "x"
    severity: WARNING
    pattern: x
    metadata:
      cwe:
        - "CWE-22"
        - "CWE-23"
      owasp:
        - "A01"
)");
    ASSERT_TRUE(pack.has_value());
    EXPECT_EQ(*pack->rules[0].metadata.primary_cwe, "CWE-22");
    EXPECT_EQ(*pack->rules[0].metadata.primary_owasp,
              euxis::security::OwaspCategory::A01_BrokenAccessControl);
}

TEST(Loader, EmptyLanguagesMeansUniversal) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: secret
    message: "Looks like a secret"
    severity: WARNING
    pattern: AWS_ACCESS_KEY_ID
)");
    ASSERT_TRUE(pack.has_value());
    EXPECT_TRUE(pack->rules[0].languages.empty());
}

TEST(Loader, MultipleRulesPreserveOrder) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: r1
    message: "rule 1"
    severity: INFO
    pattern: foo
  - id: r2
    message: "rule 2"
    severity: ERROR
    pattern: bar
  - id: r3
    message: "rule 3"
    severity: WARNING
    pattern: baz
)");
    ASSERT_TRUE(pack.has_value());
    ASSERT_EQ(pack->rules.size(), 3U);
    EXPECT_EQ(pack->rules[0].id, "r1");
    EXPECT_EQ(pack->rules[1].id, "r2");
    EXPECT_EQ(pack->rules[2].id, "r3");
}

TEST(Loader, MissingIdIsAnError) {
    auto pack = load_rules_yaml(R"(
rules:
  - message: "missing id"
    severity: ERROR
    pattern: x
)");
    EXPECT_FALSE(pack.has_value());
}

TEST(Loader, MissingMessageIsAnError) {
    auto pack = load_rules_yaml(R"(
rules:
  - id: no-message
    severity: ERROR
    pattern: x
)");
    EXPECT_FALSE(pack.has_value());
}

TEST(Loader, MissingRulesKeyIsAnError) {
    auto pack = load_rules_yaml("not_rules: []");
    EXPECT_FALSE(pack.has_value());
}

TEST(Loader, MalformedYamlIsAnError) {
    auto pack = load_rules_yaml("not: a [valid yaml :::");
    EXPECT_FALSE(pack.has_value());
}

TEST(Loader, UnknownSeverityFallsBackToWarning) {
    // Forgiving: rule with unknown severity still loads, just at
    // the default. The id/message are real; severity falls through.
    auto pack = load_rules_yaml(R"(
rules:
  - id: weird-severity
    message: "rule with weird severity"
    severity: WHATEVER
    pattern: x
)");
    ASSERT_TRUE(pack.has_value());
    EXPECT_EQ(pack->rules[0].severity, RuleSeverity::Warning);
}

} // namespace
} // namespace euxis::scan

// NOLINTEND(bugprone-unchecked-optional-access)
