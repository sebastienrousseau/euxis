#include <gtest/gtest.h>

#include "euxis/sbom/openvex.hpp"

#include <nlohmann/json.hpp>

namespace euxis::sbom {
namespace {

TEST(OpenVex, StatusVocab) {
    EXPECT_STREQ(vex_status_str(VexStatus::NotAffected),        "not_affected");
    EXPECT_STREQ(vex_status_str(VexStatus::Affected),           "affected");
    EXPECT_STREQ(vex_status_str(VexStatus::Fixed),              "fixed");
    EXPECT_STREQ(vex_status_str(VexStatus::UnderInvestigation), "under_investigation");
}

TEST(OpenVex, JustificationVocab) {
    EXPECT_STREQ(vex_justification_str(VexJustification::ComponentNotPresent),
                 "component_not_present");
    EXPECT_STREQ(vex_justification_str(VexJustification::VulnerableCodeNotInExecutePath),
                 "vulnerable_code_not_in_execute_path");
    EXPECT_STREQ(vex_justification_str(VexJustification::None), "");
}

TEST(OpenVex, MinimalDocEmitsContext) {
    VexDocument doc;
    doc.id = "urn:uuid:test-vex-001";
    doc.author = "euxis.co";
    auto json = to_openvex(doc);
    EXPECT_EQ(json["@context"], "https://openvex.dev/ns/v0.2.0");
    EXPECT_EQ(json["@id"],      "urn:uuid:test-vex-001");
    EXPECT_EQ(json["author"],   "euxis.co");
    EXPECT_EQ(json["role"],     "vendor");
    EXPECT_EQ(json["tooling"],  "euxis");
    EXPECT_EQ(json["version"],  1);
}

TEST(OpenVex, AutoIdWhenEmpty) {
    VexDocument doc;
    auto json = to_openvex(doc);
    EXPECT_TRUE(json["@id"].get<std::string>().starts_with("urn:uuid:"));
}

TEST(OpenVex, NotAffectedStatementCarriesJustification) {
    VexDocument doc;
    VexStatement s;
    s.vulnerability_name = "CVE-2025-12345";
    s.product_purls = {"pkg:cargo/serde@1.0.197"};
    s.status = VexStatus::NotAffected;
    s.justification = VexJustification::VulnerableCodeNotInExecutePath;
    s.impact_statement = "Reachability analysis (P1.7) shows no call to the affected function.";
    doc.statements.push_back(s);

    auto json = to_openvex(doc);
    ASSERT_EQ(json["statements"].size(), 1U);
    const auto& stmt = json["statements"][0];
    EXPECT_EQ(stmt["vulnerability"]["name"], "CVE-2025-12345");
    EXPECT_EQ(stmt["products"][0]["@id"],    "pkg:cargo/serde@1.0.197");
    EXPECT_EQ(stmt["status"],                "not_affected");
    EXPECT_EQ(stmt["justification"],         "vulnerable_code_not_in_execute_path");
    EXPECT_TRUE(stmt.contains("impact_statement"));
}

TEST(OpenVex, FixedStatementCarriesActionStatement) {
    VexDocument doc;
    VexStatement s;
    s.vulnerability_name = "GHSA-xxxx-yyyy-zzzz";
    s.product_purls = {"pkg:npm/lodash@4.17.21"};
    s.status = VexStatus::Fixed;
    s.action_statement = "Upgrade to lodash@4.17.22 or later.";
    doc.statements.push_back(s);

    auto json = to_openvex(doc);
    EXPECT_EQ(json["statements"][0]["status"],           "fixed");
    EXPECT_EQ(json["statements"][0]["action_statement"], "Upgrade to lodash@4.17.22 or later.");
}

TEST(OpenVex, UnderInvestigationDefaultsAreSafe) {
    VexDocument doc;
    VexStatement s;
    s.vulnerability_name = "CVE-2026-00001";
    s.product_purls = {"pkg:cargo/widget@2.1.0"};
    doc.statements.push_back(s);
    auto json = to_openvex(doc);
    EXPECT_EQ(json["statements"][0]["status"], "under_investigation");
    EXPECT_FALSE(json["statements"][0].contains("justification"));
    EXPECT_FALSE(json["statements"][0].contains("action_statement"));
}

TEST(OpenVex, MultipleProductsSerialise) {
    VexDocument doc;
    VexStatement s;
    s.vulnerability_name = "CVE-2026-00099";
    s.product_purls = {"pkg:npm/a@1", "pkg:npm/b@2", "pkg:npm/c@3"};
    s.status = VexStatus::Affected;
    doc.statements.push_back(s);
    auto json = to_openvex(doc);
    EXPECT_EQ(json["statements"][0]["products"].size(), 3U);
    EXPECT_EQ(json["statements"][0]["products"][2]["@id"], "pkg:npm/c@3");
}

TEST(OpenVex, TimestampPresentOnDocAndStatement) {
    VexDocument doc;
    VexStatement s;
    s.vulnerability_name = "CVE-1970-0001";
    s.product_purls = {"pkg:npm/x@0"};
    doc.statements.push_back(s);
    auto json = to_openvex(doc);
    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_TRUE(json["statements"][0].contains("timestamp"));
}

} // namespace
} // namespace euxis::sbom
