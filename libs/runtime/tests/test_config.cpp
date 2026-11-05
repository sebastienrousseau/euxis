#include <gtest/gtest.h>

#include "euxis/runtime/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::runtime {
namespace {

namespace fs = std::filesystem;

struct ScopedEnv {
    std::string key;
    std::optional<std::string> prev;
    ScopedEnv(std::string k, const std::string& v) : key(std::move(k)) {
        const char* p = std::getenv(key.c_str());
        if (p) prev = std::string(p);
        setenv(key.c_str(), v.c_str(), 1);
    }
    ~ScopedEnv() {
        if (prev) setenv(key.c_str(), prev->c_str(), 1);
        else      unsetenv(key.c_str());
    }
};

struct TmpDir {
    fs::path path;
    TmpDir() {
        path = fs::temp_directory_path() /
            ("euxis-cfg-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TmpDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& p, const std::string& contents) {
    std::ofstream f(p);
    f << contents;
}

TEST(RuntimeConfig, DefaultsAreSafe) {
    auto cfg = default_config();
    EXPECT_EQ(cfg.scan.mode, ScanMode::Check);
    EXPECT_EQ(cfg.scan.target, ".");
    EXPECT_TRUE(cfg.scan.respect_gitignore);
    EXPECT_EQ(cfg.gates.advisory_threshold, euxis::security::Severity::Medium);
    EXPECT_EQ(cfg.gates.blocking_threshold, euxis::security::Severity::High);
    EXPECT_TRUE(cfg.triage.enabled);
    EXPECT_EQ(cfg.triage.ensemble.quorum, 2);
    EXPECT_FALSE(cfg.triage.ensemble.send_source_code);
    EXPECT_EQ(cfg.evidence.signing, SigningMode::KeylessOidc);
    EXPECT_TRUE(cfg.evidence.sarif);
    EXPECT_TRUE(cfg.evidence.cyclonedx);
}

TEST(RuntimeConfig, ParseScanModeRoundTrip) {
    EXPECT_EQ(parse_scan_mode("triage"), ScanMode::Triage);
    EXPECT_EQ(parse_scan_mode("check"), ScanMode::Check);
    EXPECT_EQ(parse_scan_mode("review"), ScanMode::Review);
    EXPECT_EQ(parse_scan_mode("forensic"), ScanMode::Forensic);
    EXPECT_EQ(parse_scan_mode("nonsense"), ScanMode::Check);
    EXPECT_STREQ(scan_mode_str(ScanMode::Triage),   "triage");
    EXPECT_STREQ(scan_mode_str(ScanMode::Forensic), "forensic");
}

TEST(RuntimeConfig, LoadsValidYamlFile) {
    TmpDir dir;
    write_file(dir.path / ".euxis.yaml", R"(
version: "1"
project:
  name: euxis-self
  languages: [c, cpp, python]
scan:
  mode: review
  target: src/
  exclude: [build/**, third_party/**]
gates:
  blocking_threshold: critical
triage:
  enabled: true
  ensemble:
    providers: [claude, openai]
    quorum: 1
)");
    auto loaded = load_yaml_file(dir.path / ".euxis.yaml");
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(loaded->project.name, "euxis-self");
    EXPECT_EQ(loaded->project.languages.size(), 3U);
    EXPECT_EQ(loaded->scan.mode, ScanMode::Review);
    EXPECT_EQ(loaded->scan.target, "src/");
    EXPECT_EQ(loaded->scan.exclude.size(), 2U);
    EXPECT_EQ(loaded->gates.blocking_threshold, euxis::security::Severity::Critical);
    EXPECT_EQ(loaded->triage.ensemble.providers.size(), 2U);
    EXPECT_EQ(loaded->triage.ensemble.quorum, 1);
    ASSERT_EQ(loaded->sources.size(), 1U);
}

TEST(RuntimeConfig, RejectsUnknownSchemaVersion) {
    TmpDir dir;
    write_file(dir.path / ".euxis.yaml", "version: \"99\"\n");
    auto loaded = load_yaml_file(dir.path / ".euxis.yaml");
    EXPECT_FALSE(loaded.has_value());
}

TEST(RuntimeConfig, IgnoresUnknownFieldsForwardCompat) {
    TmpDir dir;
    write_file(dir.path / ".euxis.yaml", R"(
version: "1"
future_feature:
  enabled: true
scan:
  mode: triage
)");
    auto loaded = load_yaml_file(dir.path / ".euxis.yaml");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->scan.mode, ScanMode::Triage);
}

TEST(RuntimeConfig, EnvOverridesProviderRouting) {
    ScopedEnv e1{"EUXIS_DEFAULT_RESEARCH_PROVIDER", "openai"};
    ScopedEnv e2{"EUXIS_DEFAULT_CODING_PROVIDER",   "claude"};
    ScopedEnv e3{"EUXIS_DEFAULT_SECURITY_PROVIDER", "gemini"};
    auto cfg = default_config();
    apply_env_overrides(cfg);
    ASSERT_TRUE(cfg.providers.research);
    EXPECT_EQ(*cfg.providers.research, "openai");
    EXPECT_EQ(*cfg.providers.coding,   "claude");
    EXPECT_EQ(*cfg.providers.security, "gemini");
}

TEST(RuntimeConfig, EnvOverridesScanModeAndGates) {
    ScopedEnv e1{"EUXIS_SCAN_MODE", "forensic"};
    ScopedEnv e2{"EUXIS_GATES_BLOCKING", "critical"};
    auto cfg = default_config();
    apply_env_overrides(cfg);
    EXPECT_EQ(cfg.scan.mode, ScanMode::Forensic);
    EXPECT_EQ(cfg.gates.blocking_threshold, euxis::security::Severity::Critical);
}

TEST(RuntimeConfig, MergePreservesUnsetFields) {
    auto base = default_config();
    RuntimeConfig overlay;
    overlay.scan.mode = ScanMode::Triage;
    overlay.gates.blocking_threshold = euxis::security::Severity::Critical;
    merge_into(base, overlay);
    EXPECT_EQ(base.scan.mode, ScanMode::Triage);
    EXPECT_EQ(base.gates.blocking_threshold, euxis::security::Severity::Critical);
    EXPECT_EQ(base.evidence.signing, SigningMode::KeylessOidc);
}

TEST(RuntimeConfig, ResolveConfigUsesProjectFileWhenPresent) {
    TmpDir project_dir, home_dir;
    write_file(project_dir.path / ".euxis.yaml", R"(
version: "1"
scan:
  mode: triage
)");
    auto resolved = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->scan.mode, ScanMode::Triage);
}

TEST(RuntimeConfig, ResolveConfigUsesUserFileWhenProjectAbsent) {
    TmpDir project_dir, home_dir;
    fs::create_directories(home_dir.path / ".euxis");
    write_file(home_dir.path / ".euxis" / "config.yaml", R"(
version: "1"
scan:
  mode: review
)");
    auto resolved = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->scan.mode, ScanMode::Review);
}

TEST(RuntimeConfig, ProjectFileOverridesUserFile) {
    TmpDir project_dir, home_dir;
    fs::create_directories(home_dir.path / ".euxis");
    write_file(home_dir.path / ".euxis" / "config.yaml", R"(
version: "1"
scan:
  mode: review
)");
    write_file(project_dir.path / ".euxis.yaml", R"(
version: "1"
scan:
  mode: forensic
)");
    auto resolved = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->scan.mode, ScanMode::Forensic);
}

TEST(RuntimeConfig, DefaultEnsembleLeavesGeminiOut) {
    const auto& providers = default_config().triage.ensemble.providers;
    EXPECT_EQ(providers, (std::vector<std::string>{"claude", "openai"}));
}

TEST(RuntimeConfig, ParsesVerifyCommands) {
    TmpDir dir;
    write_file(dir.path / ".euxis.yaml", R"(
version: "1"
verify:
  - name: lint
    command: ["bash", "scripts/lint.sh", "--quick"]
  - name: no-command
  - command: ["true"]
  - name: unit
    command: ["make", "test-unit"]
)");
    auto loaded = load_yaml_file(dir.path / ".euxis.yaml");
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->verify.commands.size(), 2u) << "entries without a name or a command are skipped";
    EXPECT_EQ(loaded->verify.commands[0].name, "lint");
    EXPECT_EQ(loaded->verify.commands[0].command,
              (std::vector<std::string>{"bash", "scripts/lint.sh", "--quick"}));
    EXPECT_EQ(loaded->verify.commands[1].name, "unit");
}

TEST(RuntimeConfig, ProjectVerifyReplacesUserVerifyAndAbsentKeepsIt) {
    TmpDir project_dir, home_dir;
    fs::create_directories(home_dir.path / ".euxis");
    write_file(home_dir.path / ".euxis" / "config.yaml", R"(
version: "1"
verify:
  - name: user
    command: ["true"]
)");
    auto user_only = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(user_only.has_value());
    ASSERT_EQ(user_only->verify.commands.size(), 1u);
    EXPECT_EQ(user_only->verify.commands[0].name, "user");

    write_file(project_dir.path / ".euxis.yaml", R"(
version: "1"
verify:
  - name: project
    command: ["make", "check"]
)");
    auto both = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(both.has_value());
    ASSERT_EQ(both->verify.commands.size(), 1u);
    EXPECT_EQ(both->verify.commands[0].name, "project");
}

TEST(RuntimeConfig, ResolveReturnsDefaultsWhenNoFiles) {
    TmpDir project_dir, home_dir;
    auto resolved = resolve_config(project_dir.path, home_dir.path);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->scan.mode, ScanMode::Check);
}

} // namespace
} // namespace euxis::runtime

// NOLINTEND(bugprone-unchecked-optional-access)
