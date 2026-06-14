#include <gtest/gtest.h>

#include "euxis/cli/sarif.hpp"
#include "euxis/security/finding.hpp"

#include <nlohmann/json.hpp>

namespace euxis::cli {
namespace {

using euxis::security::CweRef;
using euxis::security::Finding;
using euxis::security::FixSuggestion;
using euxis::security::OwaspCategory;
using euxis::security::QuantumDeprecation;
using euxis::security::Severity;
using euxis::security::SourceLocation;

Finding make_finding_with_cwe() {
    Finding f;
    f.stable_fingerprint = "0123456789abcdef0123456789abcdef";
    f.rule_id = "euxis/cwe-79";
    f.message = "Unescaped user input rendered into an HTML response.";
    f.severity = Severity::High;
    f.confidence = euxis::security::Confidence::Probable;
    f.primary_location = SourceLocation{
        .path = "src/views/profile.tsx",
        .start_line = 42,
        .start_column = 5,
        .end_line = 42,
        .end_column = 60,
        .snippet = "<div>{userName}</div>",
    };
    f.cwe = CweRef{
        .id = "CWE-79",
        .short_name = "Cross-site Scripting",
        .in_top_25_2025 = true,
        .top_25_rank = 1,
    };
    f.owasp = OwaspCategory::A04_InsecureDesign;
    f.compliance_taxa = {"cra:annex-i:1.3.a"};
    return f;
}

TEST(SarifTaxa, EmitsSchemaAndVersion) {
    auto json = findings_to_sarif(std::vector<Finding>{}, "0.1.0-test");
    EXPECT_EQ(json["version"], "2.1.0");
    EXPECT_TRUE(json["$schema"].get<std::string>().find("sarif-schema-2.1.0") != std::string::npos);
    ASSERT_TRUE(json.contains("runs"));
    EXPECT_EQ(json["runs"].size(), 1U);
    EXPECT_EQ(json["runs"][0]["tool"]["driver"]["name"], "euxis");
    EXPECT_EQ(json["runs"][0]["tool"]["driver"]["version"], "0.1.0-test");
}

TEST(SarifTaxa, EmptyFindingsHasNoTaxonomies) {
    auto json = findings_to_sarif(std::vector<Finding>{});
    EXPECT_FALSE(json["runs"][0].contains("taxonomies"));
}

TEST(SarifTaxa, CweTaxonomyEmittedWhenFindingsHaveCwe) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    const auto& run = json["runs"][0];
    ASSERT_TRUE(run.contains("taxonomies"));
    bool saw_cwe = false;
    for (const auto& tax : run["taxonomies"]) {
        if (tax["name"] == "CWE") {
            saw_cwe = true;
            ASSERT_TRUE(tax.contains("taxa"));
            ASSERT_FALSE(tax["taxa"].empty());
            EXPECT_EQ(tax["taxa"][0]["id"], "CWE-79");
            EXPECT_EQ(tax["taxa"][0]["properties"]["in_top_25_2025"], true);
            EXPECT_EQ(tax["taxa"][0]["properties"]["top_25_rank"], 1);
        }
    }
    EXPECT_TRUE(saw_cwe);
}

TEST(SarifTaxa, OwaspTaxonomyEmittedWhenFindingsHaveOwasp) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    const auto& run = json["runs"][0];
    ASSERT_TRUE(run.contains("taxonomies"));
    bool saw_owasp = false;
    for (const auto& tax : run["taxonomies"]) {
        if (tax["name"] == "OWASP-Top-10-2025") {
            saw_owasp = true;
            ASSERT_FALSE(tax["taxa"].empty());
            EXPECT_EQ(tax["taxa"][0]["id"], "A04:2025");
        }
    }
    EXPECT_TRUE(saw_owasp);
}

TEST(SarifTaxa, RuleHasRelationshipToTaxa) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    const auto& rules = json["runs"][0]["tool"]["driver"]["rules"];
    ASSERT_FALSE(rules.empty());
    const auto& rule = rules[0];
    ASSERT_TRUE(rule.contains("relationships"));
    bool saw_cwe_rel = false, saw_owasp_rel = false;
    for (const auto& rel : rule["relationships"]) {
        std::string tc = rel["target"]["toolComponent"]["name"];
        if (tc == "CWE")                        { saw_cwe_rel = true; }
        else if (tc == "OWASP-Top-10-2025")     { saw_owasp_rel = true; }
    }
    EXPECT_TRUE(saw_cwe_rel);
    EXPECT_TRUE(saw_owasp_rel);
}

TEST(SarifTaxa, ResultCarriesStableFingerprint) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    const auto& result = json["runs"][0]["results"][0];
    ASSERT_TRUE(result.contains("fingerprints"));
    EXPECT_EQ(result["fingerprints"]["euxis.stable.v1"], "0123456789abcdef0123456789abcdef");
    ASSERT_TRUE(result.contains("partialFingerprints"));
}

TEST(SarifTaxa, SeverityHighMapsToErrorLevel) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    EXPECT_EQ(json["runs"][0]["results"][0]["level"], "error");
    EXPECT_EQ(json["runs"][0]["results"][0]["properties"]["severity"], "HIGH");
}

TEST(SarifTaxa, RegionCarriesStartLineColumnAndSnippet) {
    auto json = findings_to_sarif({make_finding_with_cwe()});
    const auto& result = json["runs"][0]["results"][0];
    ASSERT_TRUE(result.contains("locations"));
    const auto& region = result["locations"][0]["physicalLocation"]["region"];
    EXPECT_EQ(region["startLine"], 42);
    EXPECT_EQ(region["startColumn"], 5);
    EXPECT_EQ(region["endLine"], 42);
    EXPECT_EQ(region["endColumn"], 60);
    EXPECT_EQ(region["snippet"]["text"], "<div>{userName}</div>");
}

TEST(SarifTaxa, DeterministicFixEmittedInFixesArray) {
    Finding f = make_finding_with_cwe();
    f.fixes.push_back(FixSuggestion{
        .description = "Use escapeHtml() on userName",
        .replacement_text = "<div>{escapeHtml(userName)}</div>",
        .replacement_range = SourceLocation{
            .path = "src/views/profile.tsx",
            .start_line = 42,
            .start_column = 5,
            .end_line = 42,
            .end_column = 60,
        },
        .deterministic = true,
    });
    auto json = findings_to_sarif({f});
    const auto& result = json["runs"][0]["results"][0];
    ASSERT_TRUE(result.contains("fixes"));
    ASSERT_FALSE(result["fixes"].empty());
    EXPECT_EQ(result["fixes"][0]["description"]["text"], "Use escapeHtml() on userName");
    EXPECT_EQ(result["fixes"][0]["properties"]["deterministic"], true);
}

TEST(SarifTaxa, QuantumDeprecationSurfacedInProperties) {
    Finding f;
    f.rule_id = "euxis/cnsa-2.0/rsa-2048";
    f.message = "RSA-2048 used; CNSA 2.0 mandates ML-DSA-87 for signatures.";
    f.severity = Severity::Medium;
    f.quantum = QuantumDeprecation::Rsa;
    auto json = findings_to_sarif({f});
    const auto& result = json["runs"][0]["results"][0];
    EXPECT_EQ(result["properties"]["quantum_deprecation"],
              "RSA (quantum-vulnerable; CNSA 2.0 deprecation 2030)");
}

TEST(SarifTaxa, EnsembleVotesSerialised) {
    Finding f = make_finding_with_cwe();
    f.ensemble_votes.push_back({"claude-opus-4-7", true, 0.92F});
    f.ensemble_votes.push_back({"gpt-5", true, 0.88F});
    f.ensemble_votes.push_back({"gemini-2-5-pro", false, 0.71F});
    auto json = findings_to_sarif({f});
    const auto& votes = json["runs"][0]["results"][0]["properties"]["ensemble_votes"];
    ASSERT_EQ(votes.size(), 3U);
    EXPECT_EQ(votes[0]["provider"], "claude-opus-4-7");
    EXPECT_EQ(votes[0]["true_positive"], true);
    EXPECT_EQ(votes[2]["true_positive"], false);
}

TEST(SarifLegacy, BackwardsCompatibleApiStillWorks) {
    SarifFinding legacy{
        .rule_id = "euxis/security-01",
        .message = "agent flagged a thing",
        .level = "warning",
        .file_path = "src/main.cpp",
        .line = 10,
        .agent_id = "investigator",
        .pillar = "execution",
    };
    auto json = findings_to_sarif(std::vector<SarifFinding>{legacy});
    EXPECT_EQ(json["version"], "2.1.0");
    EXPECT_EQ(json["runs"][0]["results"][0]["ruleId"], "euxis/security-01");
    EXPECT_EQ(json["runs"][0]["results"][0]["properties"]["agent_id"], "investigator");
}

} // namespace
} // namespace euxis::cli
