#include <gtest/gtest.h>

#include "euxis/security/finding.hpp"

#include <string>
#include <string_view>

namespace euxis::security {
namespace {

TEST(Severity, TotalOrder) {
    EXPECT_LT(Severity::Info,   Severity::Low);
    EXPECT_LT(Severity::Low,    Severity::Medium);
    EXPECT_LT(Severity::Medium, Severity::High);
    EXPECT_LT(Severity::High,   Severity::Critical);
}

TEST(Severity, SarifLevelMapping) {
    EXPECT_STREQ(sarif_level(Severity::Critical), "error");
    EXPECT_STREQ(sarif_level(Severity::High),     "error");
    EXPECT_STREQ(sarif_level(Severity::Medium),   "warning");
    EXPECT_STREQ(sarif_level(Severity::Low),      "note");
    EXPECT_STREQ(sarif_level(Severity::Info),     "note");
    EXPECT_STREQ(sarif_level(Severity::None),     "none");
}

TEST(Severity, CliLabels) {
    EXPECT_STREQ(severity_label(Severity::Critical), "CRITICAL");
    EXPECT_STREQ(severity_label(Severity::High),     "HIGH");
    EXPECT_STREQ(severity_label(Severity::Medium),   "MEDIUM");
    EXPECT_STREQ(severity_label(Severity::Low),      "LOW");
    EXPECT_STREQ(severity_label(Severity::Info),     "INFO");
    EXPECT_STREQ(severity_label(Severity::None),     "NONE");
}

TEST(Severity, ParseSeverityCaseInsensitive) {
    EXPECT_EQ(parse_severity("critical"), Severity::Critical);
    EXPECT_EQ(parse_severity("HIGH"),     Severity::High);
    EXPECT_EQ(parse_severity("Medium"),   Severity::Medium);
    EXPECT_EQ(parse_severity("low"),      Severity::Low);
    EXPECT_EQ(parse_severity("INFO"),     Severity::Info);
    EXPECT_EQ(parse_severity(""),         Severity::None);
    EXPECT_EQ(parse_severity("garbage"),  Severity::None);
}

TEST(Owasp, AllCategoriesHaveIds) {
    EXPECT_STREQ(owasp_id(OwaspCategory::A01_BrokenAccessControl),              "A01:2025");
    EXPECT_STREQ(owasp_id(OwaspCategory::A03_SupplyChainFailures),              "A03:2025");
    EXPECT_STREQ(owasp_id(OwaspCategory::A10_MishandlingExceptionalConditions), "A10:2025");
    EXPECT_STREQ(owasp_id(OwaspCategory::None),                                 "");
}

TEST(Owasp, AllCategoriesHaveLabels) {
    EXPECT_STREQ(owasp_label(OwaspCategory::A03_SupplyChainFailures),
                 "Software Supply Chain Failures");
    EXPECT_STREQ(owasp_label(OwaspCategory::A10_MishandlingExceptionalConditions),
                 "Mishandling of Exceptional Conditions");
}

TEST(QuantumDeprecation, LabelsMentionCnsa) {
    using namespace std::string_view_literals;
    EXPECT_NE(std::string_view(quantum_label(QuantumDeprecation::Rsa)).find("CNSA"),   std::string_view::npos);
    EXPECT_NE(std::string_view(quantum_label(QuantumDeprecation::Ecdsa)).find("CNSA"), std::string_view::npos);
    EXPECT_NE(std::string_view(quantum_label(QuantumDeprecation::Ecdh)).find("CNSA"),  std::string_view::npos);
}

TEST(Finding, DefaultsAreSafe) {
    Finding f;
    EXPECT_EQ(f.severity,   Severity::Medium);
    EXPECT_EQ(f.confidence, Confidence::Probable);
    EXPECT_EQ(f.source,     RuleSource::EuxisCore);
    EXPECT_EQ(f.owasp,      OwaspCategory::None);
    EXPECT_EQ(f.quantum,    QuantumDeprecation::None);
    EXPECT_FALSE(f.cwe.has_value());
    EXPECT_TRUE(f.fixes.empty());
    EXPECT_TRUE(f.ensemble_votes.empty());
}

TEST(Finding, CarriesCweAndComplianceTaxa) {
    Finding f;
    f.cwe = CweRef{
        .id = "CWE-79",
        .short_name = "Improper Neutralization of Input During Web Page Generation",
        .in_top_25_2025 = true,
        .top_25_rank = 1,
    };
    f.owasp = OwaspCategory::A03_SupplyChainFailures;
    f.compliance_taxa = {"cra:annex-i:1.3.a", "pci-dss-4.0.1:6.3.2", "nist-ssdf:po.5.1"};
    EXPECT_TRUE(f.cwe.has_value());
    EXPECT_EQ(f.cwe->id, "CWE-79");
    EXPECT_EQ(f.cwe->top_25_rank, 1);
    EXPECT_EQ(f.compliance_taxa.size(), 3U);
}

} // namespace
} // namespace euxis::security
