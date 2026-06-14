/// @file
/// @brief Typed `.euxis.yaml` configuration loader.
///
/// Resolution order (high → low priority):
///   1. Process environment overrides
///   2. CLI flags (handled by the engine, applied via overlay())
///   3. Project file: `<repo-root>/.euxis.yaml`
///   4. User file: `$HOME/.euxis/config.yaml`
///   5. Built-in defaults
///
/// The schema lives at data/schemas/euxis.config.schema.json. This
/// header tracks that schema field-for-field; whenever the JSON
/// Schema gains a property, the corresponding C++ field must be
/// added here.
#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "euxis/security/finding.hpp"

namespace euxis::runtime {

enum class ScanMode {
    Triage,
    Check,
    Review,
    Forensic,
};

enum class SigningMode {
    Off,
    KeylessOidc,
    KeyBacked,
};

struct ProjectConfig {
    std::string name;
    std::string owner;
    std::vector<std::string> languages;
};

struct ScanConfig {
    ScanMode mode{ScanMode::Check};
    std::string target{"."};
    std::vector<std::string> include;
    std::vector<std::string> exclude{
        "build/**",
        "node_modules/**",
        "vendor/**",
        ".venv/**",
        "target/**",
        "dist/**",
        "*.min.*",
    };
    int max_file_size_kb{4096};
    bool follow_symlinks{false};
    bool respect_gitignore{true};
};

struct CacheConfig {
    bool enabled{true};
    std::string path{"~/.cache/euxis/scan-cache.sqlite"};
    int ttl_days{30};
};

struct CustomRule {
    std::string id;
    std::string pattern;
    std::string message;
    euxis::security::Severity severity{euxis::security::Severity::Medium};
    std::vector<std::string> languages;
    std::string cwe;
};

struct RulesConfig {
    std::vector<std::string> packs{
        "euxis/core",
        "euxis/cwe-top-25",
        "euxis/owasp-top-10",
        "euxis/cnsa-2.0",
        "euxis/slopsquatting",
    };
    std::vector<CustomRule> custom;
    std::unordered_map<std::string, euxis::security::Severity> severity_overrides;
};

struct BaselineConfig {
    std::string path{".euxis-baseline.json"};
    bool fail_on_new_only{true};
};

struct GatesConfig {
    euxis::security::Severity advisory_threshold{euxis::security::Severity::Medium};
    euxis::security::Severity blocking_threshold{euxis::security::Severity::High};
    std::chrono::seconds max_runtime{std::chrono::seconds{180}};
};

struct EnsembleConfig {
    std::vector<std::string> providers{"claude", "openai", "gemini"};
    int quorum{2};
    bool send_source_code{false};
};

struct TriageConfig {
    bool enabled{true};
    EnsembleConfig ensemble;
};

struct EvidenceConfig {
    bool emit{true};
    bool sarif{true};
    bool cyclonedx{true};
    bool spdx{true};
    bool openvex{true};
    SigningMode signing{SigningMode::KeylessOidc};
    bool rekor_upload{true};
    std::vector<std::string> frameworks{"owasp-top-10-2025"};
};

struct ProviderRouting {
    std::optional<std::string> research;
    std::optional<std::string> coding;
    std::optional<std::string> security;
};

struct RuntimeConfig {
    std::string schema_version{"1"};
    ProjectConfig project;
    ScanConfig scan;
    CacheConfig cache;
    RulesConfig rules;
    BaselineConfig baseline;
    GatesConfig gates;
    TriageConfig triage;
    EvidenceConfig evidence;
    ProviderRouting providers;

    /// Source files that contributed to this config (for diagnostics).
    std::vector<std::filesystem::path> sources;
};

struct ConfigError {
    std::string message;
    std::filesystem::path file;
    int line{0};
};

/// Built-in defaults. Always returns the same struct.
[[nodiscard]] auto default_config() noexcept -> RuntimeConfig;

/// Parse a single YAML file into a `RuntimeConfig`. Missing fields
/// keep their default values. Unknown fields are ignored (forward-
/// compatibility with newer schemas).
[[nodiscard]] auto load_yaml_file(const std::filesystem::path& path)
    -> std::expected<RuntimeConfig, ConfigError>;

/// Apply environment-variable overrides to a config. Recognised vars
/// match the convention used elsewhere in the codebase
/// (`EUXIS_DEFAULT_RESEARCH_PROVIDER` etc.).
void apply_env_overrides(RuntimeConfig& config) noexcept;

/// Merge `overlay` onto `base`. Vector and map fields are replaced
/// wholesale when present in the overlay; scalar fields follow
/// last-write-wins semantics. The merge respects the field-tag
/// "present in overlay" — currently implemented by checking against
/// the default value for the same field on a fresh `RuntimeConfig`.
void merge_into(RuntimeConfig& base, const RuntimeConfig& overlay) noexcept;

/// Resolve the effective config for `project_root` by walking the
/// precedence chain. Returns `default_config()` even when no file is
/// found.
[[nodiscard]] auto resolve_config(
    const std::filesystem::path& project_root,
    const std::filesystem::path& user_home = {})
    -> std::expected<RuntimeConfig, ConfigError>;

/// Convert a CLI string to a ScanMode. Returns Check on unknown
/// input so the caller can decide whether to error.
[[nodiscard]] auto parse_scan_mode(const std::string& s) noexcept -> ScanMode;

/// Convert ScanMode → CLI string.
[[nodiscard]] auto scan_mode_str(ScanMode m) noexcept -> const char*;

} // namespace euxis::runtime
