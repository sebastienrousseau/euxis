#include <gtest/gtest.h>

#include "euxis/scan/rule.hpp"

namespace euxis::scan {
namespace {

TEST(ParseSeverity, CanonicalVocab) {
    EXPECT_EQ(parse_severity("INFO"),    RuleSeverity::Info);
    EXPECT_EQ(parse_severity("WARNING"), RuleSeverity::Warning);
    EXPECT_EQ(parse_severity("ERROR"),   RuleSeverity::Error);
}

TEST(ParseSeverity, ForgivingVocab) {
    EXPECT_EQ(parse_severity("info"),          RuleSeverity::Info);
    EXPECT_EQ(parse_severity("informational"), RuleSeverity::Info);
    EXPECT_EQ(parse_severity("warn"),          RuleSeverity::Warning);
    EXPECT_EQ(parse_severity("Critical"),      RuleSeverity::Error);
}

TEST(ParseSeverity, UnknownReturnsNullopt) {
    EXPECT_FALSE(parse_severity("").has_value());
    EXPECT_FALSE(parse_severity("trace").has_value());
}

TEST(ToSecuritySeverity, MapsVocab) {
    using Sev = euxis::security::Severity;
    EXPECT_EQ(to_security_severity(RuleSeverity::Info),    Sev::Info);
    EXPECT_EQ(to_security_severity(RuleSeverity::Warning), Sev::Medium);
    EXPECT_EQ(to_security_severity(RuleSeverity::Error),   Sev::High);
}

TEST(ParseLanguageToken, RecognisesCanonicalAndAliases) {
    using L = euxis::parse::Language;
    EXPECT_EQ(parse_language_token("c"),          L::C);
    EXPECT_EQ(parse_language_token("cpp"),        L::Cpp);
    EXPECT_EQ(parse_language_token("c++"),        L::Cpp);
    EXPECT_EQ(parse_language_token("cxx"),        L::Cpp);
    EXPECT_EQ(parse_language_token("rust"),       L::Rust);
    EXPECT_EQ(parse_language_token("rs"),         L::Rust);
    EXPECT_EQ(parse_language_token("go"),         L::Go);
    EXPECT_EQ(parse_language_token("golang"),     L::Go);
    EXPECT_EQ(parse_language_token("python"),     L::Python);
    EXPECT_EQ(parse_language_token("py"),         L::Python);
    EXPECT_EQ(parse_language_token("javascript"), L::JavaScript);
    EXPECT_EQ(parse_language_token("js"),         L::JavaScript);
    EXPECT_EQ(parse_language_token("typescript"), L::TypeScript);
    EXPECT_EQ(parse_language_token("ts"),         L::TypeScript);
    EXPECT_EQ(parse_language_token("java"),       L::Java);
}

TEST(ParseLanguageToken, UnknownReturnsNullopt) {
    EXPECT_FALSE(parse_language_token("ruby").has_value());
    EXPECT_FALSE(parse_language_token("").has_value());
}

TEST(ParseOwaspToken, RecognisesAllTenCategories) {
    using C = euxis::security::OwaspCategory;
    EXPECT_EQ(parse_owasp_token("A01"),       C::A01_BrokenAccessControl);
    EXPECT_EQ(parse_owasp_token("A02"),       C::A02_CryptographicFailures);
    EXPECT_EQ(parse_owasp_token("A03"),       C::A03_SupplyChainFailures);
    EXPECT_EQ(parse_owasp_token("A04"),       C::A04_InsecureDesign);
    EXPECT_EQ(parse_owasp_token("A05"),       C::A05_SecurityMisconfiguration);
    EXPECT_EQ(parse_owasp_token("A06"),       C::A06_VulnerableComponents);
    EXPECT_EQ(parse_owasp_token("A07"),       C::A07_AuthenticationFailures);
    EXPECT_EQ(parse_owasp_token("A08"),       C::A08_DataIntegrityFailures);
    EXPECT_EQ(parse_owasp_token("A09"),       C::A09_LoggingMonitoringFailures);
    EXPECT_EQ(parse_owasp_token("A10"),       C::A10_MishandlingExceptionalConditions);
}

TEST(ParseOwaspToken, AcceptsYearSuffix) {
    using C = euxis::security::OwaspCategory;
    EXPECT_EQ(parse_owasp_token("A03:2025"),  C::A03_SupplyChainFailures);
    EXPECT_EQ(parse_owasp_token("A03-2025"),  C::A03_SupplyChainFailures);
    EXPECT_EQ(parse_owasp_token("A03_2025"),  C::A03_SupplyChainFailures);
    EXPECT_EQ(parse_owasp_token("a03"),       C::A03_SupplyChainFailures);
}

TEST(ParseOwaspToken, UnknownReturnsNullopt) {
    EXPECT_FALSE(parse_owasp_token("A11").has_value());
    EXPECT_FALSE(parse_owasp_token("XYZ").has_value());
    EXPECT_FALSE(parse_owasp_token("").has_value());
}

} // namespace
} // namespace euxis::scan
