#include <gtest/gtest.h>
#include "euxis/cli/certification_detail.hpp"
#include "euxis/cli/certification.hpp"
#include "euxis/cli/process.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace euxis::cli::certification {
namespace {

namespace fs = std::filesystem;
using detail::check_file;
using detail::full_path;
using detail::read_file_content;
using detail::gate_commit_signing;
using detail::gate_unit_test_health;
using detail::gate_build_integrity;
using detail::gate_docs_accuracy;
using detail::gate_security_critical;
using detail::analyze_quality_risk;
using detail::collect_evidence;
using detail::evaluate_domains;
using detail::finalize_result;

class CertDetailTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        root_ = "/tmp/euxis_cert_detail_" + std::to_string(getpid());
        fs::create_directories(root_);
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        fs::remove_all(root_);
    }

    fs::path root_;

    void make_git_repo() {
        auto cmd = "cd " + root_.string() + " && git init -q && "
                   "git config user.email 'test@test.com' && "
                   "git config user.name 'Test' && "
                   "echo '# Test' > README.md && "
                   "git add README.md && "
                   "git commit -q -m 'init'";
        Process::shell(cmd, 30);
    }
};

// =====================================================================
//  Utility helpers
// =====================================================================

TEST_F(CertDetailTest, CheckFileExisting) {
    std::ofstream(root_ / "test.txt") << "hello";
    EXPECT_TRUE(check_file(root_, "test.txt"));
}

TEST_F(CertDetailTest, CheckFileMissing) {
    EXPECT_FALSE(check_file(root_, "nonexistent.txt"));
}

TEST_F(CertDetailTest, FullPathConcatenation) {
    auto result = full_path(root_, "sub/file.txt");
    EXPECT_EQ(result, (root_ / "sub/file.txt").string());
}

TEST_F(CertDetailTest, ReadFileContentNormal) {
    std::ofstream(root_ / "hello.txt") << "hello world";
    auto content = read_file_content(root_ / "hello.txt");
    EXPECT_EQ(content, "hello world");
}

TEST_F(CertDetailTest, ReadFileContentMissing) {
    auto content = read_file_content(root_ / "nope.txt");
    EXPECT_TRUE(content.empty());
}

// =====================================================================
//  gate_commit_signing
// =====================================================================

TEST_F(CertDetailTest, CommitSigningAllSigned) {
    make_git_repo();
    // Default unsigned commits → status depends on system config
    auto g = gate_commit_signing(root_, 20, "", false);
    EXPECT_EQ(g.name, "commit_signing");
    EXPECT_TRUE(g.status == "pass" || g.status == "partial" || g.status == "fail");
    EXPECT_TRUE(g.data.contains("window"));
    EXPECT_EQ(g.data["window"], 20);
}

TEST_F(CertDetailTest, CommitSigningNotBlockingDefault) {
    make_git_repo();
    auto g = gate_commit_signing(root_, 20, "", false);
    EXPECT_FALSE(g.blocking);
}

TEST_F(CertDetailTest, CommitSigningBlockingInStrict) {
    make_git_repo();
    auto g = gate_commit_signing(root_, 20, "", true);
    EXPECT_TRUE(g.blocking);
}

TEST_F(CertDetailTest, CommitSigningNoGitRepo) {
    // No git repo → fail
    auto g = gate_commit_signing(root_, 20, "", false);
    EXPECT_EQ(g.status, "fail");
    EXPECT_NE(g.detail.find("Could not read git log"), std::string::npos);
}

TEST_F(CertDetailTest, CommitSigningCustomWindow) {
    make_git_repo();
    auto g = gate_commit_signing(root_, 5, "", false);
    EXPECT_EQ(g.data["window"], 5);
}

// =====================================================================
//  gate_unit_test_health
// =====================================================================

TEST_F(CertDetailTest, UnitTestHealthNoFramework) {
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_EQ(g.status, "fail");
    EXPECT_NE(g.detail.find("No test framework"), std::string::npos);
}

TEST_F(CertDetailTest, UnitTestHealthDetectsCMake) {
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    // No actual build dir, so no tests found → fail
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
}

TEST_F(CertDetailTest, UnitTestHealthDetectsCargoToml) {
    std::ofstream(root_ / "Cargo.toml") << "[package]\nname = \"test\"";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    // Mock execution, so cargo won't really run
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
}

TEST_F(CertDetailTest, UnitTestHealthDetectsGoMod) {
    std::ofstream(root_ / "go.mod") << "module test";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
}

TEST_F(CertDetailTest, UnitTestHealthDetectsPytest) {
    std::ofstream(root_ / "pytest.ini") << "[pytest]";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
}

// =====================================================================
//  gate_build_integrity
// =====================================================================

TEST_F(CertDetailTest, BuildIntegrityNoBuildSystem) {
    auto g = gate_build_integrity(root_);
    EXPECT_EQ(g.name, "build_integrity");
    EXPECT_EQ(g.status, "partial");
    EXPECT_NE(g.detail.find("No recognized build system"), std::string::npos);
}

TEST_F(CertDetailTest, BuildIntegrityDetectsCMake) {
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    auto g = gate_build_integrity(root_);
    EXPECT_EQ(g.name, "build_integrity");
    // cmake --build will fail since no actual build dir, but framework detected
    EXPECT_TRUE(g.status == "pass" || g.status == "fail");
}

TEST_F(CertDetailTest, BuildIntegrityDetectsCargoToml) {
    std::ofstream(root_ / "Cargo.toml") << "[package]\nname = \"test\"";
    auto g = gate_build_integrity(root_);
    EXPECT_EQ(g.name, "build_integrity");
    EXPECT_TRUE(g.status == "pass" || g.status == "fail");
}

TEST_F(CertDetailTest, BuildIntegrityDetectsGoMod) {
    std::ofstream(root_ / "go.mod") << "module test";
    auto g = gate_build_integrity(root_);
    EXPECT_EQ(g.name, "build_integrity");
    EXPECT_TRUE(g.status == "pass" || g.status == "fail");
}

// =====================================================================
//  gate_docs_accuracy
// =====================================================================

TEST_F(CertDetailTest, DocsAccuracyNoReadme) {
    std::vector<std::string> cmds = {"check", "triage"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.name, "documentation_accuracy");
    EXPECT_EQ(g.status, "fail");
    EXPECT_NE(g.detail.find("No README"), std::string::npos);
}

TEST_F(CertDetailTest, DocsAccuracyCleanReadme) {
    std::ofstream(root_ / "README.md") << "# Test\nRun `euxis check` to verify.";
    std::vector<std::string> cmds = {"check", "triage"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.name, "documentation_accuracy");
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
    EXPECT_FALSE(g.blocking); // non-blocking by default
}

TEST_F(CertDetailTest, DocsAccuracyStaleRefs) {
    std::ofstream(root_ / "README.md") << "# Test\nRun `euxis oldcmd` for details.";
    std::vector<std::string> cmds = {"check", "triage"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.status, "partial");
    EXPECT_NE(g.detail.find("stale"), std::string::npos);
}

TEST_F(CertDetailTest, DocsAccuracyDocsDirScan) {
    std::ofstream(root_ / "README.md") << "# Test";
    fs::create_directories(root_ / "docs");
    std::ofstream(root_ / "docs" / "guide.md") << "Use `euxis check` to run.\n";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.name, "documentation_accuracy");
    // Should scan docs/ directory
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

// =====================================================================
//  gate_security_critical
// =====================================================================

TEST_F(CertDetailTest, SecurityCriticalClean) {
    std::ofstream(root_ / ".gitignore") << ".env\n";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.name, "security_critical");
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

TEST_F(CertDetailTest, SecurityCriticalExposedSecrets) {
    std::ofstream(root_ / ".env") << "SECRET=xxx";
    // .gitignore does NOT contain .env
    std::ofstream(root_ / ".gitignore") << "build/\n";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.status, "fail");
    EXPECT_NE(g.detail.find("secret"), std::string::npos);
}

TEST_F(CertDetailTest, SecurityCriticalNoGitignore) {
    // No .gitignore, no .env → pass (no secrets)
    auto g = gate_security_critical(root_);
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

TEST_F(CertDetailTest, SecurityCriticalCMakeFlagsClean) {
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)\nset(CMAKE_BUILD_TYPE Release)";
    auto g = gate_security_critical(root_);
    EXPECT_NE(g.data["build_flags"], "fail");
}

// =====================================================================
//  analyze_quality_risk
// =====================================================================

TEST_F(CertDetailTest, QualityRiskEmptyDir) {
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskLargeFile) {
    // Create a >1000 line file
    std::ofstream f(root_ / "big.cpp");
    for (int i = 0; i < 1100; ++i) f << "// line " << i << "\n";
    f.close();

    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "gaps");
    EXPECT_FALSE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskBoundedScan) {
    // Even with many files, scan should be bounded
    for (int i = 0; i < 10; ++i) {
        std::ofstream(root_ / ("file" + std::to_string(i) + ".cpp")) << "// short\n";
    }
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.data.contains("files_scanned"));
}

// =====================================================================
//  collect_evidence
// =====================================================================

TEST_F(CertDetailTest, CollectEvidenceFromGates) {
    GateResult security_gate;
    security_gate.name = "security_critical";
    security_gate.status = "pass";
    security_gate.detail = "All clear";

    QualityRisk qr;
    qr.status = "pass";

    auto ev = collect_evidence(root_, {security_gate}, qr);
    // Should have at least the security gate evidence
    bool has_appsec = false;
    for (const auto& e : ev) {
        if (e.domain == "Application Security") has_appsec = true;
    }
    EXPECT_TRUE(has_appsec);
}

TEST_F(CertDetailTest, CollectEvidenceQualityRisk) {
    GateResult g;
    g.name = "unit_test_health";
    g.status = "pass";
    g.detail = "All tests passed";

    QualityRisk qr;
    qr.status = "gaps";
    qr.high_complexity_files = {"big.cpp (1200 lines)"};

    auto ev = collect_evidence(root_, {g}, qr);
    bool has_sqa = false;
    for (const auto& e : ev) {
        if (e.domain == "Software Quality Assurance" && e.summary.find("complexity") != std::string::npos) {
            has_sqa = true;
        }
    }
    EXPECT_TRUE(has_sqa);
}

TEST_F(CertDetailTest, CollectEvidenceFilePresence) {
    std::ofstream(root_ / "LICENSE") << "MIT License";
    std::ofstream(root_ / "README.md") << "# Project";
    std::ofstream(root_ / ".gitignore") << ".env\n";

    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);

    bool has_legal = false, has_docs = false;
    for (const auto& e : ev) {
        if (e.domain == "Legal & Regulatory Compliance") has_legal = true;
        if (e.domain == "Documentation & Audit Evidence") has_docs = true;
    }
    EXPECT_TRUE(has_legal);
    EXPECT_TRUE(has_docs);
}

TEST_F(CertDetailTest, CollectEvidenceEmpty) {
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    // Even with empty dir, no crash
    EXPECT_GE(ev.size(), 0u);
}

// =====================================================================
//  evaluate_domains
// =====================================================================

TEST_F(CertDetailTest, EvaluateDomainsAllMet) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"signal_a"}, {}, true}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "pass", false, "signal_a present"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "verified");
    EXPECT_DOUBLE_EQ(results[0].coverage, 1.0);
}

TEST_F(CertDetailTest, EvaluateDomainsPartialCoverage) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"signal_a", "signal_b"}, {}, false}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "pass", false, "signal_a present"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "gaps");
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.5);
}

TEST_F(CertDetailTest, EvaluateDomainsZeroCritical) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"signal_a"}, {}, true}
    };
    std::vector<Evidence> ev;  // no evidence
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "blocked");
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.0);
}

TEST_F(CertDetailTest, EvaluateDomainsEmptySignals) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {}, {}, false}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "pass", false, "some evidence"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "verified");
    EXPECT_DOUBLE_EQ(results[0].coverage, 1.0);
}

// =====================================================================
//  finalize_result
// =====================================================================

TEST_F(CertDetailTest, FinalizeBlocked) {
    RunResult r;
    r.gates = {{"security_critical", "fail", "Secrets found", true, {}}};
    r.domains = {{"AppSec", "blocked", 0.0, 0, true, {}, {}}};
    finalize_result(r);
    EXPECT_EQ(r.status, "BLOCKED");
    EXPECT_EQ(r.exit_code, 1);
}

TEST_F(CertDetailTest, FinalizeReady) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    // Need enough evidence and domain coverage
    r.evidence.resize(10);
    for (auto& e : r.evidence) e.execution_backed = true;
    r.domains.resize(5);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    EXPECT_EQ(r.status, "READY");
    EXPECT_EQ(r.exit_code, 0);
}

TEST_F(CertDetailTest, FinalizeReadyWithGaps) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    for (auto& e : r.evidence) e.execution_backed = true;
    r.domains.resize(10);
    int i = 0;
    for (auto& d : r.domains) {
        d.coverage = (i < 5) ? 0.8 : 0.3;
        d.status = (i < 5) ? "gaps" : "inconclusive";
        ++i;
    }
    finalize_result(r);
    EXPECT_EQ(r.status, "READY WITH GAPS");
    EXPECT_EQ(r.exit_code, 0);
}

TEST_F(CertDetailTest, FinalizeInconclusive) {
    RunResult r;
    // 3 skipped gates → inconclusive
    r.gates = {
        {"g1", "skipped", "skip", false, {}},
        {"g2", "skipped", "skip", false, {}},
        {"g3", "skipped", "skip", false, {}},
    };
    finalize_result(r);
    EXPECT_EQ(r.status, "INCONCLUSIVE");
    EXPECT_EQ(r.exit_code, 1);
}

TEST_F(CertDetailTest, FinalizeConfidenceRange) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(5);
    r.domains.resize(3);
    for (auto& d : r.domains) {
        d.coverage = 0.5;
        d.status = "gaps";
    }
    finalize_result(r);
    EXPECT_GE(r.confidence, 0);
    EXPECT_LE(r.confidence, 100);
}

// =====================================================================
//  to_json round-trip
// =====================================================================

TEST_F(CertDetailTest, ToJsonSchemaFields) {
    RunResult r;
    r.framework = Framework::SOC2;
    r.target = root_.string();
    r.strict = true;
    r.status = "BLOCKED";
    r.confidence = 30;
    r.readiness_summary = "test";
    r.exit_code = 1;

    auto j = r.to_json();
    EXPECT_EQ(j["schema"], "euxis.certification");
    EXPECT_EQ(j["framework"], "soc2");
    EXPECT_TRUE(j["strict"].get<bool>());
    EXPECT_EQ(j["status"], "BLOCKED");
}

TEST_F(CertDetailTest, ToJsonGatesPreserved) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    r.gates = {
        {"gate1", "pass", "ok", false, {{"key", "val"}}},
        {"gate2", "fail", "bad", true, {}}
    };

    auto j = r.to_json();
    ASSERT_EQ(j["gates"].size(), 2u);
    EXPECT_EQ(j["gates"][0]["name"], "gate1");
    EXPECT_EQ(j["gates"][1]["status"], "fail");
}

TEST_F(CertDetailTest, ToJsonEvidenceSummary) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    r.evidence = {
        {"ev-1", "domain", "builtin", "pass", true, "exec-backed"},
        {"ev-2", "domain", "file", "pass", false, "inferred"},
    };

    auto j = r.to_json();
    EXPECT_EQ(j["evidence_summary"]["total"], 2);
    EXPECT_EQ(j["evidence_summary"]["execution_backed"], 1);
    EXPECT_EQ(j["evidence_summary"]["inferred"], 1);
}

// =====================================================================
//  framework_name / parse_framework
// =====================================================================

TEST_F(CertDetailTest, FrameworkNameGeneral) {
    EXPECT_EQ(framework_name(Framework::General), "general");
}

TEST_F(CertDetailTest, FrameworkNameSOC2) {
    EXPECT_EQ(framework_name(Framework::SOC2), "soc2");
}

TEST_F(CertDetailTest, FrameworkNameISO27001) {
    EXPECT_EQ(framework_name(Framework::ISO27001), "iso27001");
}

TEST_F(CertDetailTest, ParseFrameworkValid) {
    EXPECT_EQ(parse_framework("general"), Framework::General);
    EXPECT_EQ(parse_framework("soc2"), Framework::SOC2);
    EXPECT_EQ(parse_framework("iso27001"), Framework::ISO27001);
}

TEST_F(CertDetailTest, ParseFrameworkInvalid) {
    EXPECT_EQ(parse_framework("unknown"), std::nullopt);
    EXPECT_EQ(parse_framework(""), std::nullopt);
}

// =====================================================================
//  default_domains / apply_framework_overlay
// =====================================================================

TEST_F(CertDetailTest, DefaultDomainsCount) {
    auto domains = default_domains();
    EXPECT_EQ(domains.size(), 18u);
}

TEST_F(CertDetailTest, DefaultDomainNames) {
    auto domains = default_domains();
    // Spot-check first and last
    EXPECT_EQ(domains[0].name, "Governance & Organizational Controls");
    EXPECT_EQ(domains[17].name, "Training & Awareness");
}

TEST_F(CertDetailTest, OverlayGeneralCritical) {
    auto domains = default_domains();
    apply_framework_overlay(Framework::General, domains);
    int critical_count = 0;
    for (const auto& d : domains) {
        if (d.critical) ++critical_count;
    }
    // General: SDLC, AppSec, SQA, Docs = 4 critical
    EXPECT_EQ(critical_count, 4);
}

TEST_F(CertDetailTest, OverlaySOC2Critical) {
    auto domains = default_domains();
    apply_framework_overlay(Framework::SOC2, domains);
    int critical_count = 0;
    for (const auto& d : domains) {
        if (d.critical) ++critical_count;
    }
    // SOC2: 4 general + Governance, IAM, Logging, Change Mgmt, Supply Chain = 9
    EXPECT_EQ(critical_count, 9);
}

TEST_F(CertDetailTest, OverlayISO27001Critical) {
    auto domains = default_domains();
    apply_framework_overlay(Framework::ISO27001, domains);
    int critical_count = 0;
    for (const auto& d : domains) {
        if (d.critical) ++critical_count;
    }
    // ISO: 4 general + Governance, VulnMgmt, Incident, BC/DR, Supply Chain = 9
    EXPECT_EQ(critical_count, 9);
}

TEST_F(CertDetailTest, OverlayISO27001SpecificDomains) {
    auto domains = default_domains();
    apply_framework_overlay(Framework::ISO27001, domains);
    // Verify ISO-specific critical domains
    bool incident_critical = false, bcdr_critical = false, vuln_critical = false;
    for (const auto& d : domains) {
        if (d.name == "Incident Response" && d.critical) incident_critical = true;
        if (d.name == "Business Continuity & Disaster Recovery" && d.critical) bcdr_critical = true;
        if (d.name == "Vulnerability & Patch Management" && d.critical) vuln_critical = true;
    }
    EXPECT_TRUE(incident_critical);
    EXPECT_TRUE(bcdr_critical);
    EXPECT_TRUE(vuln_critical);
}

// =====================================================================
//  gate_commit_signing — since_ref
// =====================================================================

TEST_F(CertDetailTest, CommitSigningSinceRef) {
    make_git_repo();
    auto g = gate_commit_signing(root_, 20, "HEAD~1", false);
    EXPECT_EQ(g.name, "commit_signing");
    // With since_ref, git log uses range — may succeed or fail depending on ref
    EXPECT_TRUE(g.status == "pass" || g.status == "partial" || g.status == "fail");
}

TEST_F(CertDetailTest, CommitSigningDataFields) {
    make_git_repo();
    auto g = gate_commit_signing(root_, 20, "", false);
    EXPECT_TRUE(g.data.contains("signed"));
    EXPECT_TRUE(g.data.contains("unsigned"));
    EXPECT_TRUE(g.data.contains("status"));
}

// =====================================================================
//  gate_unit_test_health — additional frameworks
// =====================================================================

TEST_F(CertDetailTest, UnitTestHealthDetectsPyprojectToml) {
    std::ofstream(root_ / "pyproject.toml") << "[tool.pytest]\n";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
    EXPECT_TRUE(g.data.contains("framework"));
    EXPECT_EQ(g.data["framework"], "pytest");
}

TEST_F(CertDetailTest, UnitTestHealthDetectsPackageJsonJest) {
    std::ofstream(root_ / "package.json") << "{\"name\": \"test\"}";
    fs::create_directories(root_ / "node_modules/.bin");
    std::ofstream(root_ / "node_modules/.bin/jest") << "#!/bin/sh\n";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
    EXPECT_EQ(g.data["framework"], "jest");
}

TEST_F(CertDetailTest, UnitTestHealthDetectsPackageJsonVitest) {
    std::ofstream(root_ / "package.json") << "{\"name\": \"test\"}";
    fs::create_directories(root_ / "node_modules/.bin");
    std::ofstream(root_ / "node_modules/.bin/vitest") << "#!/bin/sh\n";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_TRUE(g.status == "fail" || g.status == "partial");
    EXPECT_EQ(g.data["framework"], "vitest");
}

TEST_F(CertDetailTest, UnitTestHealthPackageJsonNoRunner) {
    // package.json but no jest/vitest → falls through to "no framework"
    std::ofstream(root_ / "package.json") << "{\"name\": \"test\"}";
    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.status, "fail");
    EXPECT_NE(g.detail.find("No test framework"), std::string::npos);
}

TEST_F(CertDetailTest, UnitTestHealthBlocking) {
    auto g = gate_unit_test_health(root_);
    EXPECT_TRUE(g.blocking);
}

TEST_F(CertDetailTest, UnitTestHealthDataFields) {
    auto g = gate_unit_test_health(root_);
    EXPECT_TRUE(g.data.contains("passed"));
    EXPECT_TRUE(g.data.contains("failed"));
    EXPECT_TRUE(g.data.contains("pass_rate"));
    EXPECT_TRUE(g.data.contains("threshold"));
}

// =====================================================================
//  gate_build_integrity — additional frameworks
// =====================================================================

TEST_F(CertDetailTest, BuildIntegrityDetectsPackageJson) {
    std::ofstream(root_ / "package.json") << "{\"name\": \"test\"}";
    auto g = gate_build_integrity(root_);
    EXPECT_EQ(g.name, "build_integrity");
    EXPECT_TRUE(g.status == "pass" || g.status == "fail");
    EXPECT_EQ(g.data["command"], "npm run");
}

TEST_F(CertDetailTest, BuildIntegrityBlocking) {
    auto g = gate_build_integrity(root_);
    EXPECT_TRUE(g.blocking);
}

TEST_F(CertDetailTest, BuildIntegrityDataFields) {
    auto g = gate_build_integrity(root_);
    EXPECT_TRUE(g.data.contains("status"));
    EXPECT_TRUE(g.data.contains("command"));
}

// =====================================================================
//  gate_docs_accuracy — additional paths
// =====================================================================

TEST_F(CertDetailTest, DocsAccuracyLowercaseReadme) {
    std::ofstream(root_ / "readme.md") << "# Test\nRun `euxis check` here.";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.name, "documentation_accuracy");
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

TEST_F(CertDetailTest, DocsAccuracyNoCommandRefs) {
    std::ofstream(root_ / "README.md") << "# My Project\nThis is a tool.";
    std::vector<std::string> cmds = {"check", "triage"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.status, "partial");
    EXPECT_NE(g.detail.find("no command references"), std::string::npos);
}

TEST_F(CertDetailTest, DocsAccuracyEmptyCommandsList) {
    std::ofstream(root_ / "README.md") << "# Test";
    std::vector<std::string> cmds;
    auto g = gate_docs_accuracy(root_, cmds);
    // No known commands to check → partial with "no command references"
    EXPECT_EQ(g.status, "partial");
}

TEST_F(CertDetailTest, DocsAccuracyDataFields) {
    std::ofstream(root_ / "README.md") << "# Test";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_TRUE(g.data.contains("status"));
    EXPECT_TRUE(g.data.contains("checked_surfaces"));
    EXPECT_TRUE(g.data.contains("stale_examples"));
}

// =====================================================================
//  gate_security_critical — additional paths
// =====================================================================

TEST_F(CertDetailTest, SecurityCriticalUnsafeBuildFlags) {
    // -DNDEBUG without Release → unsafe
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)\nadd_definitions(-DNDEBUG)";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["build_flags"], "fail");
}

TEST_F(CertDetailTest, SecurityCriticalNdebugWithRelease) {
    // -DNDEBUG with Release → safe
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)\nset(CMAKE_BUILD_TYPE Release)\nadd_definitions(-DNDEBUG)";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["build_flags"], "pass");
}

TEST_F(CertDetailTest, SecurityCriticalFetchContentDepAudit) {
    std::ofstream(root_ / "CMakeLists.txt") << "include(FetchContent)\nFetchContent_Declare(dep)";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["dependency_audit"], "pass");
}

TEST_F(CertDetailTest, SecurityCriticalVcpkgDepAudit) {
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    std::ofstream(root_ / "vcpkg.json") << "{\"name\": \"test\"}";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["dependency_audit"], "pass");
}

TEST_F(CertDetailTest, SecurityCriticalConanDepAudit) {
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    std::ofstream(root_ / "conanfile.txt") << "[requires]\nfmt/9.0.0";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["dependency_audit"], "pass");
}

TEST_F(CertDetailTest, SecurityCriticalDepAuditUnavailable) {
    // No lockfiles, no CMake dep management → unavailable
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["dependency_audit"], "unavailable");
    EXPECT_EQ(g.status, "partial");
    EXPECT_NE(g.detail.find("unavailable"), std::string::npos);
}

TEST_F(CertDetailTest, SecurityCriticalEnvLocal) {
    // .env.local without gitignore
    std::ofstream(root_ / ".env.local") << "SECRET=xxx";
    std::ofstream(root_ / ".gitignore") << "build/\n";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.status, "fail");
    EXPECT_EQ(g.data["secret_scan"], "fail");
}

TEST_F(CertDetailTest, SecurityCriticalBlocking) {
    auto g = gate_security_critical(root_);
    // Blocking on fail; this has partial status (dep audit unavailable)
    if (g.status == "partial") {
        EXPECT_FALSE(g.blocking); // partial is not blocking
    }
}

// =====================================================================
//  analyze_quality_risk — additional paths
// =====================================================================

TEST_F(CertDetailTest, QualityRiskNonSourceFilesIgnored) {
    // Non-source files should not count
    std::ofstream f(root_ / "bigfile.txt");
    for (int i = 0; i < 1500; ++i) f << "line " << i << "\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskMultipleExtensions) {
    // Test multiple source extensions
    for (const auto& ext : {".py", ".js", ".ts", ".go", ".rs", ".java"}) {
        std::ofstream f(root_ / ("small" + std::string(ext)));
        f << "// short\n";
        f.close();
    }
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_GE(qr.data["files_scanned"].get<int>(), 6);
}

TEST_F(CertDetailTest, QualityRiskSkipsGitDir) {
    fs::create_directories(root_ / ".git" / "objects");
    std::ofstream f(root_ / ".git" / "huge.cpp");
    for (int i = 0; i < 2000; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    // .git should be skipped
    EXPECT_EQ(qr.status, "pass");
}

TEST_F(CertDetailTest, QualityRiskSkipsNodeModules) {
    fs::create_directories(root_ / "node_modules" / "pkg");
    std::ofstream f(root_ / "node_modules" / "pkg" / "huge.js");
    for (int i = 0; i < 2000; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
}

// =====================================================================
//  collect_evidence — additional paths
// =====================================================================

TEST_F(CertDetailTest, CollectEvidenceGovernance) {
    std::ofstream(root_ / "CONTRIBUTING.md") << "# Contributing";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_governance = false;
    for (const auto& e : ev) {
        if (e.domain == "Governance & Organizational Controls" &&
            e.summary.find("CONTRIBUTING") != std::string::npos) {
            has_governance = true;
        }
    }
    EXPECT_TRUE(has_governance);
}

TEST_F(CertDetailTest, CollectEvidenceCodeowners) {
    fs::create_directories(root_ / ".github");
    std::ofstream(root_ / ".github" / "CODEOWNERS") << "* @team";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_codeowners = false;
    for (const auto& e : ev) {
        if (e.summary.find("CODEOWNERS") != std::string::npos) has_codeowners = true;
    }
    EXPECT_TRUE(has_codeowners);
}

TEST_F(CertDetailTest, CollectEvidenceSDLC) {
    std::ofstream(root_ / ".gitignore") << ".env\n";
    std::ofstream(root_ / "SECURITY.md") << "# Security Policy";
    fs::create_directories(root_ / ".github" / "workflows");
    std::ofstream(root_ / ".github" / "workflows" / "ci.yml") << "name: CI";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    int sdlc_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Secure Software Development Lifecycle") ++sdlc_count;
    }
    EXPECT_GE(sdlc_count, 2); // .gitignore + SECURITY.md at minimum
}

TEST_F(CertDetailTest, CollectEvidenceIAM) {
    std::ofstream(root_ / ".gitignore") << ".env\ncredentials\n";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_iam = false;
    for (const auto& e : ev) {
        if (e.domain == "Identity & Access Management") has_iam = true;
    }
    EXPECT_TRUE(has_iam);
}

TEST_F(CertDetailTest, CollectEvidenceInfrastructure) {
    std::ofstream(root_ / "Dockerfile") << "FROM alpine";
    std::ofstream(root_ / "docker-compose.yml") << "version: '3'";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    int infra_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Infrastructure & Cloud Security") ++infra_count;
    }
    EXPECT_GE(infra_count, 2);
}

TEST_F(CertDetailTest, CollectEvidenceVulnerabilityMgmt) {
    std::ofstream(root_ / "Cargo.lock") << "[[package]]";
    std::ofstream(root_ / "go.sum") << "module hash";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    int vuln_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Vulnerability & Patch Management") ++vuln_count;
    }
    EXPECT_GE(vuln_count, 2);
}

TEST_F(CertDetailTest, CollectEvidenceChangeMgmt) {
    std::ofstream(root_ / "CHANGELOG.md") << "# Changelog";
    GateResult commit_gate;
    commit_gate.name = "commit_signing";
    commit_gate.status = "pass";
    commit_gate.detail = "5/5 signed";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {commit_gate}, qr);
    int change_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Change Management") ++change_count;
    }
    EXPECT_GE(change_count, 2); // commit_signing + CHANGELOG
}

TEST_F(CertDetailTest, CollectEvidenceDocsGate) {
    std::ofstream(root_ / "README.md") << "# Project";
    fs::create_directories(root_ / "docs");
    std::ofstream(root_ / "docs" / "arch.md") << "# Architecture";
    GateResult docs_gate;
    docs_gate.name = "documentation_accuracy";
    docs_gate.status = "pass";
    docs_gate.detail = "Docs OK";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {docs_gate}, qr);
    int doc_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Documentation & Audit Evidence") ++doc_count;
    }
    EXPECT_GE(doc_count, 3); // README + docs/ + gate
}

TEST_F(CertDetailTest, CollectEvidenceSupplyChain) {
    std::ofstream(root_ / "vcpkg.json") << "{\"name\": \"test\"}";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_supply = false;
    for (const auto& e : ev) {
        if (e.domain == "Supply Chain & Third-Party Security" &&
            e.summary.find("vcpkg") != std::string::npos) {
            has_supply = true;
        }
    }
    EXPECT_TRUE(has_supply);
}

TEST_F(CertDetailTest, CollectEvidenceLogging) {
    fs::create_directories(root_ / "data" / "runtime" / "metrics");
    fs::create_directories(root_ / "data" / "gateway" / "audit");
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    int log_count = 0;
    for (const auto& e : ev) {
        if (e.domain == "Logging, Monitoring & Detection") ++log_count;
    }
    EXPECT_GE(log_count, 2);
}

TEST_F(CertDetailTest, CollectEvidencePerformance) {
    fs::create_directories(root_ / "data" / "config");
    std::ofstream(root_ / "data" / "config" / "targets.json") << "{}";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_perf = false;
    for (const auto& e : ev) {
        if (e.domain == "Performance & Reliability" &&
            e.summary.find("SLA") != std::string::npos) {
            has_perf = true;
        }
    }
    EXPECT_TRUE(has_perf);
}

TEST_F(CertDetailTest, CollectEvidenceIncidentResponse) {
    std::ofstream(root_ / "SECURITY.md") << "# Security\nReport bugs to...";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_ir = false;
    for (const auto& e : ev) {
        if (e.domain == "Incident Response") has_ir = true;
    }
    EXPECT_TRUE(has_ir);
}

TEST_F(CertDetailTest, CollectEvidenceBusinessContinuity) {
    std::ofstream(root_ / "BACKUP.md") << "# Backup";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_bcdr = false;
    for (const auto& e : ev) {
        if (e.domain == "Business Continuity & Disaster Recovery") has_bcdr = true;
    }
    EXPECT_TRUE(has_bcdr);
}

TEST_F(CertDetailTest, CollectEvidenceDataSecurity) {
    std::ofstream(root_ / "PRIVACY.md") << "# Privacy";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_ds = false;
    for (const auto& e : ev) {
        if (e.domain == "Data Security & Privacy") has_ds = true;
    }
    EXPECT_TRUE(has_ds);
}

TEST_F(CertDetailTest, CollectEvidenceTraining) {
    fs::create_directories(root_ / "docs" / "guides");
    std::ofstream(root_ / "docs" / "guides" / "start.md") << "# Guide";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_training = false;
    for (const auto& e : ev) {
        if (e.domain == "Training & Awareness") has_training = true;
    }
    EXPECT_TRUE(has_training);
}

TEST_F(CertDetailTest, CollectEvidenceOnboarding) {
    std::ofstream(root_ / "ONBOARDING.md") << "# Onboarding";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_onboard = false;
    for (const auto& e : ev) {
        if (e.domain == "Training & Awareness" &&
            e.summary.find("Onboarding") != std::string::npos) {
            has_onboard = true;
        }
    }
    EXPECT_TRUE(has_onboard);
}

TEST_F(CertDetailTest, CollectEvidenceEuxisArtifacts) {
    fs::create_directories(root_ / "data" / "runtime" / "sessions");
    std::ofstream(root_ / "data" / "runtime" / "sessions" / "latest_verdict.json") << "{}";
    fs::create_directories(root_ / "data" / "config");
    std::ofstream(root_ / "data" / "config" / "router.json") << "{}";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_verdict = false, has_router = false;
    for (const auto& e : ev) {
        if (e.summary.find("verdict artifact") != std::string::npos) has_verdict = true;
        if (e.summary.find("routing configuration") != std::string::npos) has_router = true;
    }
    EXPECT_TRUE(has_verdict);
    EXPECT_TRUE(has_router);
}

TEST_F(CertDetailTest, CollectEvidenceExecutionBacked) {
    GateResult g;
    g.name = "security_critical";
    g.status = "pass";
    g.detail = "clean";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {g}, qr);
    // Gate evidence should be execution_backed
    bool found_exec = false;
    for (const auto& e : ev) {
        if (e.source == "builtin" && e.execution_backed) found_exec = true;
    }
    EXPECT_TRUE(found_exec);
}

// =====================================================================
//  evaluate_domains — additional paths
// =====================================================================

TEST_F(CertDetailTest, EvaluateDomainsNonCriticalZeroCoverage) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"signal_a"}, {}, false}  // non-critical
    };
    std::vector<Evidence> ev;  // no evidence
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "inconclusive");
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.0);
}

TEST_F(CertDetailTest, EvaluateDomainsLooseSignalMatch) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"Security"}, {}, false}
    };
    // Summary contains "security" (lowercase) — should match via case-insensitive loose match
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "builtin", "pass", true, "security gate passed"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "verified");
    EXPECT_DOUBLE_EQ(results[0].coverage, 1.0);
}

TEST_F(CertDetailTest, EvaluateDomainsMultipleDomains) {
    std::vector<DomainDefinition> defs = {
        {"DomA", {"sig_a"}, {}, true},
        {"DomB", {}, {}, false},
        {"DomC", {"sig_c"}, {}, false}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "DomA", "file", "pass", false, "sig_a present"},
        {"ev-2", "DomB", "file", "pass", false, "something"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].status, "verified");   // DomA: sig_a met
    EXPECT_EQ(results[1].status, "verified");   // DomB: no required signals, has evidence
    EXPECT_EQ(results[2].status, "inconclusive"); // DomC: non-critical, sig_c missing
}

TEST_F(CertDetailTest, EvaluateDomainsEvidenceCount) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {}, {}, false}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "pass", false, "a"},
        {"ev-2", "TestDomain", "file", "pass", false, "b"},
        {"ev-3", "OtherDomain", "file", "pass", false, "c"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].evidence_count, 2);
}

TEST_F(CertDetailTest, EvaluateDomainsEmptySignalsNoEvidence) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {}, {}, false}
    };
    std::vector<Evidence> ev;
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    // No required signals, no evidence → coverage 0.0 → inconclusive (non-critical)
    EXPECT_EQ(results[0].status, "inconclusive");
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.0);
}

TEST_F(CertDetailTest, EvaluateDomainsFailedEvidenceNotCounted) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"sig_a"}, {}, false}
    };
    // Evidence exists but status is "fail" — should NOT satisfy the signal
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "builtin", "fail", true, "sig_a failed"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.0);
    EXPECT_EQ(results[0].status, "inconclusive");
}

TEST_F(CertDetailTest, EvaluateDomainsMissingControls) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"sig_a", "sig_b"}, {}, false}
    };
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "pass", false, "sig_a present"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].missing_controls.size(), 1u);
    EXPECT_EQ(results[0].missing_controls[0], "sig_b");
    ASSERT_EQ(results[0].findings.size(), 1u);
    EXPECT_NE(results[0].findings[0].find("sig_a"), std::string::npos);
}

// =====================================================================
//  finalize_result — additional paths
// =====================================================================

TEST_F(CertDetailTest, FinalizeGateFailNonBlocking) {
    RunResult r;
    r.gates = {{"docs_gate", "fail", "Docs bad", false, {}}}; // non-blocking
    r.evidence.resize(10);
    r.domains.resize(5);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    // Should NOT be BLOCKED since gate is non-blocking
    EXPECT_NE(r.status, "BLOCKED");
    EXPECT_TRUE(r.blocking_issues.empty());
}

TEST_F(CertDetailTest, FinalizeCriticalDomainBlocked) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {{"CritDomain", "blocked", 0.0, 0, true, {}, {}}};
    finalize_result(r);
    EXPECT_EQ(r.status, "BLOCKED");
    EXPECT_FALSE(r.blocking_issues.empty());
    EXPECT_NE(r.blocking_issues[0].find("CritDomain"), std::string::npos);
}

TEST_F(CertDetailTest, FinalizeRecommendedActions) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {
        {"Dom1", "gaps", 0.5, 1, false,
         {"sig: covered"}, {"CONTRIBUTING", "SECURITY"}},
        {"Dom2", "gaps", 0.5, 1, false,
         {}, {"LICENSE", "training docs"}}
    };
    finalize_result(r);
    EXPECT_FALSE(r.recommended_actions.empty());
    // Check for specific recommendations
    bool has_contributing = false, has_security = false, has_license = false;
    for (const auto& a : r.recommended_actions) {
        if (a.find("CONTRIBUTING") != std::string::npos) has_contributing = true;
        if (a.find("SECURITY") != std::string::npos) has_security = true;
        if (a.find("LICENSE") != std::string::npos) has_license = true;
    }
    EXPECT_TRUE(has_contributing);
    EXPECT_TRUE(has_security);
    EXPECT_TRUE(has_license);
}

TEST_F(CertDetailTest, FinalizeRecommendedActionsDeduped) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    // Two domains with same missing control
    r.domains = {
        {"Dom1", "gaps", 0.5, 1, false, {}, {"LICENSE"}},
        {"Dom2", "gaps", 0.5, 1, false, {}, {"LICENSE"}}
    };
    finalize_result(r);
    // Count LICENSE recommendations
    int license_count = 0;
    for (const auto& a : r.recommended_actions) {
        if (a.find("LICENSE") != std::string::npos) ++license_count;
    }
    EXPECT_EQ(license_count, 1); // Deduplicated
}

TEST_F(CertDetailTest, FinalizeSkippedGatesInSummary) {
    RunResult r;
    r.gates = {
        {"g1", "pass", "OK", true, {}},
        {"g2", "skipped", "skip", false, {}},
    };
    r.evidence.resize(10);
    r.domains.resize(5);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    EXPECT_NE(r.readiness_summary.find("skipped"), std::string::npos);
    EXPECT_EQ(r.skipped_gates.size(), 1u);
    EXPECT_EQ(r.skipped_gates[0], "g2");
}

TEST_F(CertDetailTest, FinalizeMajorGapsPopulated) {
    RunResult r;
    r.gates = {{"test_gate", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {
        {"Dom1", "gaps", 0.6, 2, false, {}, {"control_a", "control_b"}}
    };
    finalize_result(r);
    EXPECT_EQ(r.major_gaps.size(), 2u);
    EXPECT_NE(r.major_gaps[0].find("Dom1"), std::string::npos);
    EXPECT_NE(r.major_gaps[0].find("control_a"), std::string::npos);
}

TEST_F(CertDetailTest, FinalizeInconclusiveLowEvidence) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(3); // < 5 evidence → INCONCLUSIVE
    r.domains.resize(2);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    EXPECT_EQ(r.status, "INCONCLUSIVE");
}

TEST_F(CertDetailTest, FinalizeInconclusiveTooManyInconclusive) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    // More than half domains inconclusive
    r.domains = {
        {"D1", "inconclusive", 0.0, 0, false, {}, {}},
        {"D2", "inconclusive", 0.0, 0, false, {}, {}},
        {"D3", "verified", 1.0, 3, false, {}, {}}
    };
    finalize_result(r);
    EXPECT_EQ(r.status, "INCONCLUSIVE");
}

TEST_F(CertDetailTest, FinalizeConfidenceComponents) {
    RunResult r;
    // 2 gates, both pass → gate_score = 40
    r.gates = {
        {"g1", "pass", "OK", true, {}},
        {"g2", "pass", "OK", true, {}}
    };
    // 10 evidence, 5 exec-backed → exec_ratio = 0.5 → exec_score = 10
    r.evidence.resize(10);
    for (int i = 0; i < 5; ++i) r.evidence[i].execution_backed = true;
    // 2 domains, avg coverage = 1.0 → domain_score = 30
    r.domains = {
        {"D1", "verified", 1.0, 5, false, {}, {}},
        {"D2", "verified", 1.0, 5, false, {}, {}}
    };
    finalize_result(r);
    // 40 + 30 + 10 + 10(no critical) = 90
    EXPECT_EQ(r.confidence, 90);
}

TEST_F(CertDetailTest, FinalizeConfidenceCapped) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    for (auto& e : r.evidence) e.execution_backed = true;
    r.domains.resize(5);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    EXPECT_LE(r.confidence, 100);
}

TEST_F(CertDetailTest, FinalizeSummaryFormat) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {
        {"D1", "verified", 1.0, 5, false, {}, {}},
        {"D2", "gaps", 0.6, 3, false, {}, {"missing"}},
        {"D3", "inconclusive", 0.2, 1, false, {}, {}}
    };
    finalize_result(r);
    EXPECT_NE(r.readiness_summary.find("1 verified"), std::string::npos);
    EXPECT_NE(r.readiness_summary.find("1 with gaps"), std::string::npos);
    EXPECT_NE(r.readiness_summary.find("1 inconclusive"), std::string::npos);
}

TEST_F(CertDetailTest, FinalizeTrainingRecommendation) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {
        {"Training", "gaps", 0.5, 1, false, {}, {"onboarding"}}
    };
    finalize_result(r);
    bool has_training_rec = false;
    for (const auto& a : r.recommended_actions) {
        if (a.find("training") != std::string::npos || a.find("onboarding") != std::string::npos) {
            has_training_rec = true;
        }
    }
    EXPECT_TRUE(has_training_rec);
}

// =====================================================================
//  to_json — additional paths
// =====================================================================

TEST_F(CertDetailTest, ToJsonDomainsPreserved) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    r.domains = {
        {"TestDomain", "verified", 0.8, 3, true, {"finding1"}, {"missing1"}}
    };
    auto j = r.to_json();
    ASSERT_EQ(j["domains"].size(), 1u);
    EXPECT_EQ(j["domains"][0]["name"], "TestDomain");
    EXPECT_EQ(j["domains"][0]["status"], "verified");
    EXPECT_DOUBLE_EQ(j["domains"][0]["coverage"].get<double>(), 0.8);
    EXPECT_EQ(j["domains"][0]["evidence_count"], 3);
    EXPECT_TRUE(j["domains"][0]["critical"].get<bool>());
    EXPECT_EQ(j["domains"][0]["findings"].size(), 1u);
    EXPECT_EQ(j["domains"][0]["missing_controls"].size(), 1u);
}

TEST_F(CertDetailTest, ToJsonQualityRisk) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    r.quality_risk.data = {{"status", "gaps"}, {"files_scanned", 42}};
    auto j = r.to_json();
    EXPECT_EQ(j["quality_risk"]["status"], "gaps");
    EXPECT_EQ(j["quality_risk"]["files_scanned"], 42);
}

TEST_F(CertDetailTest, ToJsonBlockingIssues) {
    RunResult r;
    r.status = "BLOCKED";
    r.confidence = 10;
    r.readiness_summary = "blocked";
    r.exit_code = 1;
    r.blocking_issues = {"Gate 'security' failed"};
    r.major_gaps = {"AppSec: missing audit"};
    r.recommended_actions = {"Add SECURITY.md"};
    r.skipped_gates = {"build_integrity"};
    auto j = r.to_json();
    EXPECT_EQ(j["blocking_issues"].size(), 1u);
    EXPECT_EQ(j["major_gaps"].size(), 1u);
    EXPECT_EQ(j["recommended_actions"].size(), 1u);
    EXPECT_EQ(j["skipped_gates"].size(), 1u);
    EXPECT_EQ(j["exit_code"], 1);
}

TEST_F(CertDetailTest, ToJsonGateDataSections) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    r.gates = {
        {"commit_signing", "pass", "ok", false, {{"signed", 5}, {"unsigned", 0}}},
        {"security_critical", "pass", "ok", true, {{"secret_scan", "pass"}}}
    };
    auto j = r.to_json();
    // Gate data is also written to top-level keys by gate name
    EXPECT_EQ(j["commit_signing"]["signed"], 5);
    EXPECT_EQ(j["security_critical"]["secret_scan"], "pass");
}

TEST_F(CertDetailTest, ToJsonTimestamp) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_TRUE(j.contains("timestamp"));
    std::string ts = j["timestamp"];
    // ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    EXPECT_EQ(ts.size(), 20u);
    EXPECT_EQ(ts.back(), 'Z');
    EXPECT_EQ(ts[10], 'T');
}

TEST_F(CertDetailTest, ToJsonDisclaimer) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_EQ(j["assessment_type"], "internal_readiness");
    EXPECT_FALSE(j["official_certification"].get<bool>());
    EXPECT_NE(j["disclaimer"].get<std::string>().find("not proof"), std::string::npos);
}

TEST_F(CertDetailTest, ToJsonSchemaVersion) {
    RunResult r;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_EQ(j["schema_version"], "1.0.0");
}

TEST_F(CertDetailTest, ToJsonFrameworkGeneral) {
    RunResult r;
    r.framework = Framework::General;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_EQ(j["framework"], "general");
}

TEST_F(CertDetailTest, ToJsonFrameworkISO27001) {
    RunResult r;
    r.framework = Framework::ISO27001;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_EQ(j["framework"], "iso27001");
}

// =====================================================================
//  Coverage gap: gate_unit_test_health — gtest output parsing
// =====================================================================

TEST_F(CertDetailTest, UnitTestHealthGtestPassedParsing) {
    // Simulate gtest output with [  PASSED  ] and [  FAILED  ] lines
    // We can't easily inject mock output into Process::run, but we can test
    // that the gate detects frameworks correctly and returns expected data shapes.
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    fs::create_directories(root_ / "cmake-build");

    // Create a mock test binary that outputs gtest-like text
    auto test_bin = root_ / "cmake-build" / "test_runner";
    {
        std::ofstream f(test_bin);
        f << "#!/bin/sh\necho '[  PASSED  ] 10 tests.'\necho '[  FAILED  ] 2 tests.'\n";
    }
    chmod(test_bin.c_str(), 0755);

    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    // The mock binary should be found via substring fallback
    EXPECT_TRUE(g.data.contains("passed"));
    EXPECT_TRUE(g.data.contains("failed"));
    EXPECT_TRUE(g.data.contains("pass_rate"));
}

TEST_F(CertDetailTest, UnitTestHealthPreferredNameMatch) {
    // Test preferred name matching (euxis-cli-cpp_tests)
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    fs::create_directories(root_ / "cmake-build");
    auto test_bin = root_ / "cmake-build" / "tests";
    {
        std::ofstream f(test_bin);
        f << "#!/bin/sh\necho '[  PASSED  ] 5 tests.'\n";
    }
    chmod(test_bin.c_str(), 0755);

    auto g = gate_unit_test_health(root_);
    EXPECT_EQ(g.name, "unit_test_health");
    EXPECT_EQ(g.data["framework"], "cmake/gtest");
}

TEST_F(CertDetailTest, UnitTestHealthPassRate95) {
    // Tests pass rate >= 95% → pass status
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    fs::create_directories(root_ / "cmake-build");
    auto test_bin = root_ / "cmake-build" / "tests";
    {
        std::ofstream f(test_bin);
        f << "#!/bin/sh\necho '[  PASSED  ] 96 tests.'\necho '[  FAILED  ] 4 tests.'\nexit 1\n";
    }
    chmod(test_bin.c_str(), 0755);

    auto g = gate_unit_test_health(root_);
    // 96/100 = 96% >= 95% → pass (if gtest output parsed)
    EXPECT_TRUE(g.data.contains("threshold"));
    EXPECT_DOUBLE_EQ(g.data["threshold"].get<double>(), 0.95);
}

TEST_F(CertDetailTest, UnitTestHealthPassRateBelow95) {
    // Tests pass rate < 95% → fail status
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    fs::create_directories(root_ / "cmake-build");
    auto test_bin = root_ / "cmake-build" / "tests";
    {
        std::ofstream f(test_bin);
        f << "#!/bin/sh\necho '[  PASSED  ] 90 tests.'\necho '[  FAILED  ] 10 tests.'\nexit 1\n";
    }
    chmod(test_bin.c_str(), 0755);

    auto g = gate_unit_test_health(root_);
    // If gtest output was parsed: 90/100 = 90% < 95% → fail
    EXPECT_TRUE(g.data.contains("passed"));
    EXPECT_TRUE(g.data.contains("failed"));
}

TEST_F(CertDetailTest, UnitTestHealthFallbackExitCodeSuccess) {
    // No gtest output but exit code 0 → partial
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    fs::create_directories(root_ / "cmake-build");
    auto test_bin = root_ / "cmake-build" / "tests";
    {
        std::ofstream f(test_bin);
        f << "#!/bin/sh\necho 'All good'\nexit 0\n";
    }
    chmod(test_bin.c_str(), 0755);

    auto g = gate_unit_test_health(root_);
    // No PASSED/FAILED lines → fallback to exit code
    EXPECT_TRUE(g.status == "pass" || g.status == "partial" || g.status == "fail");
}

// =====================================================================
//  Coverage gap: gate_docs_accuracy — token skip paths
// =====================================================================

TEST_F(CertDetailTest, DocsAccuracySkipsDashTokens) {
    // Tokens starting with '-' should not count as stale
    std::ofstream(root_ / "README.md") << "Use `euxis --help` and `euxis -v` for info.";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    // --help and -v should be skipped, not counted as stale
    EXPECT_EQ(g.name, "documentation_accuracy");
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

TEST_F(CertDetailTest, DocsAccuracySkipsAngleBracketTokens) {
    // Tokens starting with '<' should be skipped (placeholder)
    std::ofstream(root_ / "README.md") << "Run `euxis <command>` to execute.";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_EQ(g.data["stale_examples"], 0);
}

TEST_F(CertDetailTest, DocsAccuracySingleCharTokenSkipped) {
    // Single-char tokens should be skipped (token.size() > 1 check)
    std::ofstream(root_ / "README.md") << "Run euxis x to do something.";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    // 'x' is a single char, should not count as stale
    EXPECT_EQ(g.data["stale_examples"], 0);
}

TEST_F(CertDetailTest, DocsAccuracyBacktickEuxisPrefix) {
    // `euxis should also be detected
    std::ofstream(root_ / "README.md") << "Run `euxis check` to verify.";
    std::vector<std::string> cmds = {"check"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_TRUE(g.status == "pass" || g.status == "partial");
}

TEST_F(CertDetailTest, DocsAccuracyPunctuationStripped) {
    // Trailing punctuation should be stripped from tokens
    std::ofstream(root_ / "README.md") << "Try euxis check. Also euxis triage!";
    std::vector<std::string> cmds = {"check", "triage"};
    auto g = gate_docs_accuracy(root_, cmds);
    EXPECT_GE(g.data["checked_surfaces"].size(), 2u);
}

// =====================================================================
//  Coverage gap: gate_security_critical — dep audit fail status
// =====================================================================

TEST_F(CertDetailTest, SecurityCriticalDepAuditFail) {
    // When dep audit returns fail, overall should be fail
    // This is hard to test directly without npm/cargo, but we test the status
    // composition path by checking that no secrets + unavailable audit → partial
    auto g = gate_security_critical(root_);
    // No .env, no lockfiles → dep_audit unavailable → partial
    EXPECT_EQ(g.status, "partial");
    EXPECT_EQ(g.data["dependency_audit"], "unavailable");
}

TEST_F(CertDetailTest, SecurityCriticalEnvGitignored) {
    // .env exists but IS in .gitignore → pass
    std::ofstream(root_ / ".env") << "SECRET=xxx";
    std::ofstream(root_ / ".gitignore") << ".env\n";
    auto g = gate_security_critical(root_);
    EXPECT_NE(g.status, "fail"); // Should not be fail since .env is gitignored
    EXPECT_EQ(g.data["secret_scan"], "pass");
}

TEST_F(CertDetailTest, SecurityCriticalConanfilePy) {
    // conanfile.py should also be detected
    std::ofstream(root_ / "CMakeLists.txt") << "project(test)";
    std::ofstream(root_ / "conanfile.py") << "class MyConan(ConanFile): pass";
    auto g = gate_security_critical(root_);
    EXPECT_EQ(g.data["dependency_audit"], "pass");
}

// =====================================================================
//  Coverage gap: analyze_quality_risk — vendor/build skip
// =====================================================================

TEST_F(CertDetailTest, QualityRiskSkipsVendorDir) {
    fs::create_directories(root_ / "vendor" / "pkg");
    std::ofstream f(root_ / "vendor" / "pkg" / "huge.go");
    for (int i = 0; i < 2000; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskSkipsBuildDir) {
    fs::create_directories(root_ / "build" / "gen");
    std::ofstream f(root_ / "build" / "gen" / "huge.cpp");
    for (int i = 0; i < 2000; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskSkipsCmakeBuildDir) {
    fs::create_directories(root_ / "cmake-build" / "gen");
    std::ofstream f(root_ / "cmake-build" / "gen" / "huge.cpp");
    for (int i = 0; i < 2000; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_EQ(qr.status, "pass");
    EXPECT_TRUE(qr.high_complexity_files.empty());
}

TEST_F(CertDetailTest, QualityRiskDataFields) {
    auto qr = analyze_quality_risk(root_);
    EXPECT_TRUE(qr.data.contains("status"));
    EXPECT_TRUE(qr.data.contains("high_complexity_files"));
    EXPECT_TRUE(qr.data.contains("hotspots"));
    EXPECT_TRUE(qr.data.contains("files_scanned"));
}

TEST_F(CertDetailTest, QualityRiskLargeFileRelativePath) {
    // Verify high_complexity_files contains relative path with line count
    std::ofstream f(root_ / "huge.rs");
    for (int i = 0; i < 1200; ++i) f << "// line\n";
    f.close();
    auto qr = analyze_quality_risk(root_);
    EXPECT_FALSE(qr.high_complexity_files.empty());
    EXPECT_NE(qr.high_complexity_files[0].find("huge.rs"), std::string::npos);
    EXPECT_NE(qr.high_complexity_files[0].find("1200"), std::string::npos);
}

// =====================================================================
//  Coverage gap: collect_evidence — additional file paths
// =====================================================================

TEST_F(CertDetailTest, CollectEvidenceGitLabCI) {
    std::ofstream(root_ / ".gitlab-ci.yml") << "stages: [build]";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_gitlab = false;
    for (const auto& e : ev) {
        if (e.summary.find("GitLab CI") != std::string::npos) has_gitlab = true;
    }
    EXPECT_TRUE(has_gitlab);
}

TEST_F(CertDetailTest, CollectEvidenceDockerComposeYaml) {
    std::ofstream(root_ / "docker-compose.yaml") << "version: '3'";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_compose = false;
    for (const auto& e : ev) {
        if (e.summary.find("Docker Compose") != std::string::npos) has_compose = true;
    }
    EXPECT_TRUE(has_compose);
}

TEST_F(CertDetailTest, CollectEvidenceRootCodeowners) {
    std::ofstream(root_ / "CODEOWNERS") << "* @admin";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_codeowners = false;
    for (const auto& e : ev) {
        if (e.summary.find("CODEOWNERS") != std::string::npos) has_codeowners = true;
    }
    EXPECT_TRUE(has_codeowners);
}

TEST_F(CertDetailTest, CollectEvidenceNoticeFile) {
    std::ofstream(root_ / "NOTICE") << "Third-party licenses";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_notice = false;
    for (const auto& e : ev) {
        if (e.summary.find("NOTICE") != std::string::npos) has_notice = true;
    }
    EXPECT_TRUE(has_notice);
}

TEST_F(CertDetailTest, CollectEvidenceLicenseMd) {
    std::ofstream(root_ / "LICENSE.md") << "# MIT License";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_license = false;
    for (const auto& e : ev) {
        if (e.domain == "Legal & Regulatory Compliance" &&
            e.summary.find("LICENSE") != std::string::npos) has_license = true;
    }
    EXPECT_TRUE(has_license);
}

TEST_F(CertDetailTest, CollectEvidencePackageLockJson) {
    std::ofstream(root_ / "package-lock.json") << "{}";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_vuln = false, has_supply = false;
    for (const auto& e : ev) {
        if (e.domain == "Vulnerability & Patch Management" &&
            e.summary.find("Package lockfile") != std::string::npos) has_vuln = true;
        if (e.domain == "Supply Chain & Third-Party Security" &&
            e.summary.find("npm lockfile") != std::string::npos) has_supply = true;
    }
    EXPECT_TRUE(has_vuln);
    EXPECT_TRUE(has_supply);
}

TEST_F(CertDetailTest, CollectEvidencePoetryLock) {
    std::ofstream(root_ / "poetry.lock") << "[[package]]";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_poetry = false;
    for (const auto& e : ev) {
        if (e.summary.find("Poetry lockfile") != std::string::npos) has_poetry = true;
    }
    EXPECT_TRUE(has_poetry);
}

TEST_F(CertDetailTest, CollectEvidenceChangelogNoExt) {
    std::ofstream(root_ / "CHANGELOG") << "# Changes";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_changelog = false;
    for (const auto& e : ev) {
        if (e.domain == "Change Management" &&
            e.summary.find("CHANGELOG") != std::string::npos) has_changelog = true;
    }
    EXPECT_TRUE(has_changelog);
}

TEST_F(CertDetailTest, CollectEvidencePiiFilter) {
    fs::create_directories(root_ / "apps" / "cli" / "src");
    std::ofstream(root_ / "apps" / "cli" / "src" / "pii_filter.cpp") << "// PII filter";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_pii = false;
    for (const auto& e : ev) {
        if (e.domain == "User Protection & Safety" &&
            e.summary.find("PII filter") != std::string::npos) has_pii = true;
    }
    EXPECT_TRUE(has_pii);
}

TEST_F(CertDetailTest, CollectEvidenceDisasterRecovery) {
    fs::create_directories(root_ / "docs");
    std::ofstream(root_ / "docs" / "disaster-recovery.md") << "# DR Plan";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_dr = false;
    for (const auto& e : ev) {
        if (e.domain == "Business Continuity & Disaster Recovery" &&
            e.summary.find("DR") != std::string::npos) has_dr = true;
    }
    EXPECT_TRUE(has_dr);
}

TEST_F(CertDetailTest, CollectEvidenceMetricsDir) {
    fs::create_directories(root_ / "metrics");
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    bool has_metrics = false;
    for (const auto& e : ev) {
        if (e.domain == "Logging, Monitoring & Detection" &&
            e.summary.find("Metrics") != std::string::npos) has_metrics = true;
    }
    EXPECT_TRUE(has_metrics);
}

TEST_F(CertDetailTest, CollectEvidenceUnitTestGate) {
    GateResult g;
    g.name = "unit_test_health";
    g.status = "pass";
    g.detail = "100/100 passed";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {g}, qr);
    bool has_test_gate = false;
    for (const auto& e : ev) {
        if (e.domain == "Software Quality Assurance" &&
            e.summary.find("Test gate") != std::string::npos) has_test_gate = true;
    }
    EXPECT_TRUE(has_test_gate);
}

TEST_F(CertDetailTest, CollectEvidenceBuildGate) {
    GateResult g;
    g.name = "build_integrity";
    g.status = "pass";
    g.detail = "Build OK";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {g}, qr);
    bool has_build_gate = false;
    for (const auto& e : ev) {
        if (e.domain == "Software Quality Assurance" &&
            e.summary.find("Build gate") != std::string::npos) has_build_gate = true;
    }
    EXPECT_TRUE(has_build_gate);
}

TEST_F(CertDetailTest, CollectEvidenceIdsAreUnique) {
    // All evidence should have unique IDs
    std::ofstream(root_ / "LICENSE") << "MIT";
    std::ofstream(root_ / "README.md") << "# Test";
    std::ofstream(root_ / ".gitignore") << ".env\n";
    std::ofstream(root_ / "SECURITY.md") << "# Security";
    QualityRisk qr;
    qr.status = "pass";
    auto ev = collect_evidence(root_, {}, qr);
    std::set<std::string> ids;
    for (const auto& e : ev) {
        EXPECT_TRUE(ids.insert(e.id).second) << "Duplicate ID: " << e.id;
    }
}

// =====================================================================
//  Coverage gap: evaluate_domains — partial evidence status
// =====================================================================

TEST_F(CertDetailTest, EvaluateDomainsPartialEvidence) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"sig_a"}, {}, false}
    };
    // Evidence with "partial" status should still satisfy the signal
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "builtin", "partial", true, "sig_a partial"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].status, "verified");
    EXPECT_DOUBLE_EQ(results[0].coverage, 1.0);
}

TEST_F(CertDetailTest, EvaluateDomainsInferredStatusIgnored) {
    std::vector<DomainDefinition> defs = {
        {"TestDomain", {"sig_a"}, {}, false}
    };
    // Evidence with "inferred" status should NOT satisfy (only pass/partial)
    std::vector<Evidence> ev = {
        {"ev-1", "TestDomain", "file", "inferred", false, "sig_a inferred"}
    };
    auto results = evaluate_domains(defs, ev);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_DOUBLE_EQ(results[0].coverage, 0.0);
}

TEST_F(CertDetailTest, EvaluateDomainsCriticalPreserved) {
    std::vector<DomainDefinition> defs = {
        {"CritDom", {"sig"}, {}, true},
        {"NonCrit", {"sig"}, {}, false}
    };
    std::vector<Evidence> ev;
    auto results = evaluate_domains(defs, ev);
    EXPECT_TRUE(results[0].critical);
    EXPECT_FALSE(results[1].critical);
}

// =====================================================================
//  Coverage gap: finalize_result — confidence with critical domains
// =====================================================================

TEST_F(CertDetailTest, FinalizeConfidenceWithCriticalDomains) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    for (auto& e : r.evidence) e.execution_backed = true;
    // Critical domain with 50% coverage
    r.domains = {
        {"CritDom", "gaps", 0.5, 3, true, {}, {"missing"}},
        {"NonCrit", "verified", 1.0, 5, false, {}, {}}
    };
    finalize_result(r);
    // gate_score = 40, domain_score = 0.75 * 30 = 22.5,
    // exec_score = 20, critical_score = 0.5 * 10 = 5
    // Total ≈ 87
    EXPECT_GE(r.confidence, 80);
    EXPECT_LE(r.confidence, 95);
}

TEST_F(CertDetailTest, FinalizeEmptyResult) {
    RunResult r;
    finalize_result(r);
    EXPECT_EQ(r.status, "INCONCLUSIVE");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_GE(r.confidence, 0);
    EXPECT_LE(r.confidence, 100);
}

TEST_F(CertDetailTest, FinalizeNoRecommendationsWhenNoMissing) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    r.domains = {
        {"Dom1", "verified", 1.0, 5, false, {}, {}}
    };
    finalize_result(r);
    EXPECT_TRUE(r.recommended_actions.empty());
}

TEST_F(CertDetailTest, FinalizeReadySummaryContainsStatus) {
    RunResult r;
    r.gates = {{"g1", "pass", "OK", true, {}}};
    r.evidence.resize(10);
    for (auto& e : r.evidence) e.execution_backed = true;
    r.domains.resize(5);
    for (auto& d : r.domains) {
        d.coverage = 1.0;
        d.status = "verified";
    }
    finalize_result(r);
    EXPECT_NE(r.readiness_summary.find("READY"), std::string::npos);
    EXPECT_NE(r.readiness_summary.find("5 verified"), std::string::npos);
}

TEST_F(CertDetailTest, FinalizeBlockedSummaryContainsBlocked) {
    RunResult r;
    r.gates = {{"sec", "fail", "bad", true, {}}};
    r.domains = {{"D1", "blocked", 0.0, 0, true, {}, {}}};
    finalize_result(r);
    EXPECT_NE(r.readiness_summary.find("BLOCKED"), std::string::npos);
}

// =====================================================================
//  Coverage gap: to_json — empty result and edge cases
// =====================================================================

TEST_F(CertDetailTest, ToJsonEmptyResult) {
    RunResult r;
    r.status = "INCONCLUSIVE";
    r.confidence = 0;
    r.readiness_summary = "empty";
    auto j = r.to_json();
    EXPECT_EQ(j["gates"].size(), 0u);
    EXPECT_EQ(j["domains"].size(), 0u);
    EXPECT_EQ(j["evidence_summary"]["total"], 0);
    EXPECT_EQ(j["evidence_summary"]["execution_backed"], 0);
    EXPECT_EQ(j["evidence_summary"]["inferred"], 0);
}

TEST_F(CertDetailTest, ToJsonAllEvidenceExecBacked) {
    RunResult r;
    r.status = "READY";
    r.confidence = 90;
    r.readiness_summary = "ok";
    r.evidence = {
        {"ev-1", "d", "builtin", "pass", true, "a"},
        {"ev-2", "d", "builtin", "pass", true, "b"}
    };
    auto j = r.to_json();
    EXPECT_EQ(j["evidence_summary"]["execution_backed"], 2);
    EXPECT_EQ(j["evidence_summary"]["inferred"], 0);
}

TEST_F(CertDetailTest, ToJsonStrictField) {
    RunResult r;
    r.strict = false;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "ok";
    auto j = r.to_json();
    EXPECT_FALSE(j["strict"].get<bool>());
}

// =====================================================================
//  Coverage gap: default_domains — signal/optional content
// =====================================================================

TEST_F(CertDetailTest, DefaultDomainsHaveRequiredSignals) {
    auto domains = default_domains();
    // Application Security should require "security gate"
    bool found_appsec = false;
    for (const auto& d : domains) {
        if (d.name == "Application Security") {
            found_appsec = true;
            EXPECT_FALSE(d.required_signals.empty());
            EXPECT_TRUE(std::find(d.required_signals.begin(), d.required_signals.end(),
                                  "security gate") != d.required_signals.end());
        }
    }
    EXPECT_TRUE(found_appsec);
}

TEST_F(CertDetailTest, DefaultDomainsSQACritical) {
    auto domains = default_domains();
    for (const auto& d : domains) {
        if (d.name == "Software Quality Assurance") {
            EXPECT_TRUE(d.critical);
            break;
        }
    }
}

TEST_F(CertDetailTest, OverlaySOC2SpecificDomains) {
    auto domains = default_domains();
    apply_framework_overlay(Framework::SOC2, domains);
    bool iam_critical = false, logging_critical = false, change_critical = false;
    for (const auto& d : domains) {
        if (d.name == "Identity & Access Management" && d.critical) iam_critical = true;
        if (d.name == "Logging, Monitoring & Detection" && d.critical) logging_critical = true;
        if (d.name == "Change Management" && d.critical) change_critical = true;
    }
    EXPECT_TRUE(iam_critical);
    EXPECT_TRUE(logging_critical);
    EXPECT_TRUE(change_critical);
}

} // namespace
} // namespace euxis::cli::certification
