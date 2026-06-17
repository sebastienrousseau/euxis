#include <gtest/gtest.h>

#include "euxis/slopsquatting/guard.hpp"
#include "euxis/slopsquatting/integration.hpp"

#include "euxis/sca/scanner.hpp"

namespace euxis::slopsquatting {
namespace {

using euxis::sbom::PurlType;

euxis::sca::ScanResult make_scan_result_with(
    euxis::sca::Ecosystem eco,
    std::vector<std::pair<std::string, std::string>> entries) {
    euxis::sca::ScanResult sr;
    euxis::sca::ParsedManifest m;
    m.ecosystem = eco;
    m.source_file = "test-lockfile";
    for (auto& [name, version] : entries) {
        euxis::sca::ManifestEntry e;
        e.name = name;
        e.version = version;
        m.entries.push_back(std::move(e));
    }
    sr.manifests.push_back(std::move(m));
    return sr;
}

TEST(Integration, NoMatchesProducesNoFindings) {
    Guard g;
    g.add(PurlType::Npm, {"easy-fetch"});
    auto sr = make_scan_result_with(euxis::sca::Ecosystem::Npm, {
        {"lodash", "4.17.21"},
        {"react",  "19.0.0"},
    });
    auto findings = check_scan_result(g, sr);
    EXPECT_TRUE(findings.empty());
}

TEST(Integration, MatchEmitsSlopsqFinding) {
    Guard g;
    g.add(PurlType::Npm, {"easy-fetch"});
    auto sr = make_scan_result_with(euxis::sca::Ecosystem::Npm, {
        {"easy-fetch", "1.0.0"},
    });
    auto findings = check_scan_result(g, sr);
    ASSERT_EQ(findings.size(), 1U);
    const auto& f = findings.front();
    EXPECT_EQ(f.rule_id, "euxis/slopsquatting/EUXIS-SLOPSQ-001");
    EXPECT_EQ(f.severity, euxis::security::Severity::High);
    EXPECT_EQ(f.source, euxis::security::RuleSource::SlopsquattingGuard);
    EXPECT_EQ(f.owasp, euxis::security::OwaspCategory::A03_SupplyChainFailures);
    ASSERT_TRUE(f.cwe.has_value());
    EXPECT_EQ(f.cwe->id, "CWE-1357");
}

TEST(Integration, EcosystemScopingPreventsFalseMatch) {
    Guard g;
    g.add(PurlType::Cargo, {"easy-fetch"});  // only cargo
    auto sr = make_scan_result_with(euxis::sca::Ecosystem::Npm, {
        {"easy-fetch", "1.0.0"},
    });
    auto findings = check_scan_result(g, sr);
    EXPECT_TRUE(findings.empty());
}

TEST(Integration, FingerprintIsStable) {
    Guard g;
    g.add(PurlType::Npm, {"easy-fetch"});
    auto sr = make_scan_result_with(euxis::sca::Ecosystem::Npm, {
        {"easy-fetch", "1.0.0"},
    });
    auto a = check_scan_result(g, sr);
    auto b = check_scan_result(g, sr);
    ASSERT_EQ(a.size(), 1U);
    ASSERT_EQ(b.size(), 1U);
    EXPECT_EQ(a[0].stable_fingerprint, b[0].stable_fingerprint);
    EXPECT_EQ(a[0].stable_fingerprint.size(), 32U);
}

TEST(Integration, FingerprintDiffersByEcosystem) {
    Guard g;
    g.add(PurlType::Npm,   {"easy-fetch"});
    g.add(PurlType::Cargo, {"easy-fetch"});

    auto sr_npm = make_scan_result_with(euxis::sca::Ecosystem::Npm,
                                        {{"easy-fetch", "1.0.0"}});
    auto sr_cargo = make_scan_result_with(euxis::sca::Ecosystem::Cargo,
                                          {{"easy-fetch", "1.0.0"}});

    auto fn_npm = check_scan_result(g, sr_npm);
    auto fn_cargo = check_scan_result(g, sr_cargo);
    ASSERT_EQ(fn_npm.size(), 1U);
    ASSERT_EQ(fn_cargo.size(), 1U);
    EXPECT_NE(fn_npm[0].stable_fingerprint, fn_cargo[0].stable_fingerprint);
}

TEST(Integration, ScopedNpmPackagesUseNamespace) {
    Guard g;
    g.add(PurlType::Npm, {"@scope/widget"});

    euxis::sca::ScanResult sr;
    euxis::sca::ParsedManifest m;
    m.ecosystem = euxis::sca::Ecosystem::Npm;
    m.source_file = "lock";
    euxis::sca::ManifestEntry e;
    e.ns = "@scope";
    e.name = "widget";
    e.version = "1.0.0";
    m.entries.push_back(e);
    sr.manifests.push_back(m);

    auto findings = check_scan_result(g, sr);
    ASSERT_EQ(findings.size(), 1U);
}

TEST(Integration, MessageMentionsPackageName) {
    Guard g;
    g.add(PurlType::Pypi, {"urlib3"});
    auto sr = make_scan_result_with(euxis::sca::Ecosystem::Pypi, {
        {"urlib3", "2.0.0"},
    });
    auto findings = check_scan_result(g, sr);
    ASSERT_EQ(findings.size(), 1U);
    EXPECT_NE(findings[0].message.find("urlib3"), std::string::npos);
    EXPECT_NE(findings[0].message.find("pypi"),   std::string::npos);
}

} // namespace
} // namespace euxis::slopsquatting
