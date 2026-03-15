#include <gtest/gtest.h>
#include "euxis/cli/cmd/certify.hpp"
#include "euxis/cli/certification.hpp"
#include "euxis/cli/engine.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "euxis/cli/process.hpp"

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class CertifyCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        setenv("EUXIS_TEST_MOCK_EXECUTION", "1", 1);
        ctx_.euxis_home = "/tmp/euxis_test_certify_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/agents");
        fs::create_directories(ctx_.data_dir + "/runtime/sessions");

        // Create a minimal test repo
        repo_dir_ = "/tmp/euxis_test_certify_repo_" + std::to_string(getpid());
        fs::create_directories(repo_dir_);

        // Init git repo
        auto git_init = "cd " + repo_dir_ + " && git init -q && "
                        "git config user.email 'test@test.com' && "
                        "git config user.name 'Test' && "
                        "echo '# Test' > README.md && "
                        "git add README.md && "
                        "git commit -q -m 'init'";
        Process::shell(git_init, 30);

        // Create .gitignore
        std::ofstream(repo_dir_ + "/.gitignore") << ".env\nbuild/\n";
    }

    void TearDown() override {
        unsetenv("EUXIS_TEST_MOCK_EXECUTION");
        fs::remove_all(ctx_.euxis_home);
        fs::remove_all(repo_dir_);
    }

    Context ctx_;
    std::string repo_dir_;
};

// =====================================================================
//  Help & Usage
// =====================================================================

TEST_F(CertifyCmdTest, CertifyHelpContent) {
    // Capture stderr
    testing::internal::CaptureStderr();
    auto code = cmd_certify_readiness(ctx_, {"--help"});
    auto output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(code, 0);
    EXPECT_NE(output.find("certify-readiness"), std::string::npos);
    EXPECT_NE(output.find("--framework"), std::string::npos);
    EXPECT_NE(output.find("controls"), std::string::npos);
    EXPECT_NE(output.find("report"), std::string::npos);
    EXPECT_NE(output.find("--strict"), std::string::npos);
    EXPECT_NE(output.find("--no-build"), std::string::npos);
    EXPECT_NE(output.find("--commit-window"), std::string::npos);
    EXPECT_NE(output.find("--since-ref"), std::string::npos);
}

TEST_F(CertifyCmdTest, CertifyControlsLists18Domains) {
    testing::internal::CaptureStdout();
    auto code = cmd_certify_readiness(ctx_, {"controls"});
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(code, 0);
    EXPECT_NE(output.find("Governance"), std::string::npos);
    EXPECT_NE(output.find("Legal"), std::string::npos);
    EXPECT_NE(output.find("Software Quality"), std::string::npos);
    EXPECT_NE(output.find("Documentation"), std::string::npos);
    EXPECT_NE(output.find("Training"), std::string::npos);
    EXPECT_NE(output.find("Supply Chain"), std::string::npos);
    EXPECT_NE(output.find("Application Security"), std::string::npos);
    EXPECT_NE(output.find("Incident Response"), std::string::npos);
    // Disclaimer must be present
    EXPECT_NE(output.find("internal"), std::string::npos);
}

TEST_F(CertifyCmdTest, CertifyReportMissingFileReturns1) {
    auto code = cmd_certify_readiness(ctx_, {"report", "/tmp/nonexistent_cert_artifact.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(CertifyCmdTest, CertifyInvalidFrameworkReturns2) {
    auto code = cmd_certify_readiness(ctx_, {repo_dir_, "--framework", "bogus"});
    EXPECT_EQ(code, 2);
}

TEST_F(CertifyCmdTest, CertifyReportMalformedReturns2) {
    // Write a malformed file
    auto path = repo_dir_ + "/bad_artifact.json";
    std::ofstream(path) << "{ not json }}}";
    auto code = cmd_certify_readiness(ctx_, {"report", path});
    EXPECT_EQ(code, 2);
}

// =====================================================================
//  Artifact
// =====================================================================

TEST_F(CertifyCmdTest, CertificationArtifactSchemaPresent) {
    // Run with --no-build --no-tests --no-security to skip slow gates
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});

    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    ASSERT_TRUE(fs::exists(artifact_path));

    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);
    EXPECT_EQ(artifact["schema"], "euxis.certification");
    EXPECT_EQ(artifact["schema_version"], "1.0.0");
    EXPECT_EQ(artifact["assessment_type"], "internal_readiness");
    EXPECT_EQ(artifact["official_certification"], false);
    EXPECT_TRUE(artifact.contains("disclaimer"));
}

TEST_F(CertifyCmdTest, CertificationArtifactContains18Domains) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});

    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    ASSERT_TRUE(fs::exists(artifact_path));

    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);
    ASSERT_TRUE(artifact.contains("domains"));
    EXPECT_EQ(artifact["domains"].size(), 18u);
}

TEST_F(CertifyCmdTest, CertificationArtifactWritesLatestFile) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    EXPECT_TRUE(fs::exists(artifact_path));
}

TEST_F(CertifyCmdTest, CertificationArtifactContainsGates) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});

    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);
    ASSERT_TRUE(artifact.contains("gates"));
    // 5 gates: commit_signing, unit_test_health (skipped), build_integrity (skipped),
    //          documentation_accuracy, security_critical (skipped)
    EXPECT_EQ(artifact["gates"].size(), 5u);
}

TEST_F(CertifyCmdTest, CertificationArtifactCustomOutput) {
    auto custom_path = repo_dir_ + "/custom_cert.json";
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security", "--output", custom_path});
    EXPECT_TRUE(fs::exists(custom_path));

    std::ifstream f(custom_path);
    auto artifact = nlohmann::json::parse(f);
    EXPECT_EQ(artifact["schema"], "euxis.certification");
}

// =====================================================================
//  Frameworks
// =====================================================================

TEST_F(CertifyCmdTest, CertifyGeneralFrameworkRuns) {
    auto code = cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security",
                                             "--framework", "general"});
    EXPECT_GE(code, 0);
    EXPECT_LE(code, 2);
}

TEST_F(CertifyCmdTest, CertifySoc2HasExtraCriticalDomains) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security",
                                 "--framework", "soc2"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    // Count critical domains — SOC2 should have more than general
    int soc2_critical = 0;
    for (const auto& d : artifact["domains"]) {
        if (d.value("critical", false)) ++soc2_critical;
    }

    // Run general for comparison
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security",
                                 "--framework", "general"});
    std::ifstream f2(artifact_path);
    auto artifact2 = nlohmann::json::parse(f2);
    int general_critical = 0;
    for (const auto& d : artifact2["domains"]) {
        if (d.value("critical", false)) ++general_critical;
    }

    EXPECT_GT(soc2_critical, general_critical);
}

TEST_F(CertifyCmdTest, CertifyIsoFrameworkRuns) {
    auto code = cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security",
                                             "--framework", "iso27001"});
    EXPECT_GE(code, 0);
    EXPECT_LE(code, 2);
}

// =====================================================================
//  Gates
// =====================================================================

TEST_F(CertifyCmdTest, CertifyDocsGapsNonBlockingDefault) {
    // Remove README to trigger docs gap
    fs::remove(repo_dir_ + "/README.md");
    Process::shell("cd " + repo_dir_ + " && git add -A && git commit -q -m 'remove readme'", 30);

    (void)cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    // Should NOT be BLOCKED since docs is non-blocking by default
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    bool docs_blocking = false;
    for (const auto& g : artifact["gates"]) {
        if (g["name"] == "documentation_accuracy") {
            docs_blocking = g.value("blocking", false);
        }
    }
    EXPECT_FALSE(docs_blocking);
}

TEST_F(CertifyCmdTest, CertifyDocsGapsBlocksInStrict) {
    // Remove README
    fs::remove(repo_dir_ + "/README.md");
    Process::shell("cd " + repo_dir_ + " && git add -A && git commit -q -m 'remove readme'", 30);

    cmd_certify_readiness(ctx_, {repo_dir_, "--strict", "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    // In strict mode, docs should be blocking and the overall status should be BLOCKED
    EXPECT_EQ(artifact["status"], "BLOCKED");
}

TEST_F(CertifyCmdTest, CertifyCommitSigningNotBlockingByDefault) {
    // Commit signing gate should not be blocking in default (non-strict) mode
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    for (const auto& g : artifact["gates"]) {
        if (g["name"] == "commit_signing") {
            auto status = g.value("status", "");
            // Status depends on system config, but must be valid
            EXPECT_TRUE(status == "pass" || status == "fail" || status == "partial");
            // NOT blocking by default (regardless of signing status)
            EXPECT_FALSE(g.value("blocking", true));
        }
    }
}

TEST_F(CertifyCmdTest, CertifyNoTestsFlag) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-tests", "--no-build", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    for (const auto& g : artifact["gates"]) {
        if (g["name"] == "unit_test_health") {
            EXPECT_EQ(g["status"], "skipped");
        }
    }
}

TEST_F(CertifyCmdTest, CertifySkippedGatesReduceReadiness) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    EXPECT_TRUE(artifact.contains("skipped_gates"));
    EXPECT_GE(artifact["skipped_gates"].size(), 3u);

    // Readiness summary should mention skipped gates
    auto summary = artifact.value("readiness_summary", "");
    EXPECT_NE(summary.find("skipped"), std::string::npos);
}

TEST_F(CertifyCmdTest, CertifyCommitWindow) {
    cmd_certify_readiness(ctx_, {repo_dir_, "--commit-window", "5",
                                 "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json";
    std::ifstream f(artifact_path);
    auto artifact = nlohmann::json::parse(f);

    for (const auto& g : artifact["gates"]) {
        if (g["name"] == "commit_signing" && g.contains("data")) {
            EXPECT_EQ(g["data"].value("window", 0), 5);
        }
    }
}

// =====================================================================
//  Reporting
// =====================================================================

TEST_F(CertifyCmdTest, CertifyReportPrettyPrintsArtifact) {
    // First generate an artifact
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    auto artifact_path = (fs::path(ctx_.euxis_home) / "data" / "runtime" / "sessions" / "latest_certification.json").string();

    testing::internal::CaptureStdout();
    auto code = cmd_certify_readiness(ctx_, {"report", artifact_path});
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(code, 0);
    EXPECT_NE(output.find("CERTIFICATION"), std::string::npos);
    EXPECT_NE(output.find("internal"), std::string::npos);  // disclaimer
}

TEST_F(CertifyCmdTest, CertifyJsonOutputValid) {
    testing::internal::CaptureStdout();
    cmd_certify_readiness(ctx_, {repo_dir_, "--json", "--no-build", "--no-tests", "--no-security"});
    auto output = testing::internal::GetCapturedStdout();

    // Should be valid JSON
    nlohmann::json parsed;
    EXPECT_NO_THROW(parsed = nlohmann::json::parse(output));
    EXPECT_EQ(parsed["schema"], "euxis.certification");
}

TEST_F(CertifyCmdTest, CertifyDisclaimerInOutput) {
    testing::internal::CaptureStdout();
    cmd_certify_readiness(ctx_, {repo_dir_, "--no-build", "--no-tests", "--no-security"});
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("internal"), std::string::npos);
    EXPECT_NE(output.find("not claim"), std::string::npos);
}

// =====================================================================
//  Engine
// =====================================================================

TEST_F(CertifyCmdTest, HelpShowsCertifyReadinessInCore) {
    Engine engine(ctx_.euxis_home);
    testing::internal::CaptureStdout();
    engine.run({"help"});
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("certify-readiness"), std::string::npos);
    // Old certify should still be in Specialized
    EXPECT_NE(output.find("certify"), std::string::npos);
}

// =====================================================================
//  Certification engine unit tests
// =====================================================================

TEST(CertificationTest, FrameworkParsing) {
    EXPECT_EQ(certification::parse_framework("general"), certification::Framework::General);
    EXPECT_EQ(certification::parse_framework("soc2"), certification::Framework::SOC2);
    EXPECT_EQ(certification::parse_framework("iso27001"), certification::Framework::ISO27001);
    EXPECT_EQ(certification::parse_framework("bogus"), std::nullopt);
}

TEST(CertificationTest, FrameworkNames) {
    EXPECT_EQ(certification::framework_name(certification::Framework::General), "general");
    EXPECT_EQ(certification::framework_name(certification::Framework::SOC2), "soc2");
    EXPECT_EQ(certification::framework_name(certification::Framework::ISO27001), "iso27001");
}

TEST(CertificationTest, DefaultDomainsReturns18) {
    auto domains = certification::default_domains();
    EXPECT_EQ(domains.size(), 18u);
}

TEST(CertificationTest, GeneralOverlaySetsCorrectCritical) {
    auto domains = certification::default_domains();
    certification::apply_framework_overlay(certification::Framework::General, domains);

    int critical = 0;
    for (const auto& d : domains) {
        if (d.critical) ++critical;
    }
    // General: SDLC, AppSec, SQA, Docs = 4 critical
    EXPECT_EQ(critical, 4);
}

TEST(CertificationTest, SOC2OverlayHasMoreCritical) {
    auto general = certification::default_domains();
    certification::apply_framework_overlay(certification::Framework::General, general);
    int general_critical = 0;
    for (const auto& d : general) if (d.critical) ++general_critical;

    auto soc2 = certification::default_domains();
    certification::apply_framework_overlay(certification::Framework::SOC2, soc2);
    int soc2_critical = 0;
    for (const auto& d : soc2) if (d.critical) ++soc2_critical;

    EXPECT_GT(soc2_critical, general_critical);
}

TEST(CertificationTest, RunResultToJsonHasRequiredFields) {
    certification::RunResult r;
    r.framework = certification::Framework::General;
    r.target = "/test";
    r.strict = false;
    r.status = "READY";
    r.confidence = 50;
    r.readiness_summary = "test summary";
    r.exit_code = 0;

    auto j = r.to_json();
    EXPECT_EQ(j["schema"], "euxis.certification");
    EXPECT_EQ(j["schema_version"], "1.0.0");
    EXPECT_EQ(j["assessment_type"], "internal_readiness");
    EXPECT_EQ(j["official_certification"], false);
    EXPECT_TRUE(j.contains("disclaimer"));
    EXPECT_TRUE(j.contains("timestamp"));
    EXPECT_TRUE(j.contains("gates"));
    EXPECT_TRUE(j.contains("domains"));
    EXPECT_TRUE(j.contains("evidence_summary"));
}

} // namespace
} // namespace euxis::cli::cmd
