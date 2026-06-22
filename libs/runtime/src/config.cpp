#include "euxis/runtime/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

namespace euxis::runtime {

namespace {

using euxis::security::Severity;
using euxis::security::parse_severity;

template <typename T>
auto get_or(const YAML::Node& node, const std::string& key, T fallback) -> T {
    if (!node || !node.IsMap() || !node[key] || node[key].IsNull()) {
        return fallback;
    }
    try {
        return node[key].as<T>();
    } catch (const YAML::TypedBadConversion<T>&) {
        return fallback;
    }
}

auto get_severity(const YAML::Node& node, const std::string& key, Severity fallback) -> Severity {
    if (!node || !node.IsMap() || !node[key] || !node[key].IsScalar()) {
        return fallback;
    }
    auto s = parse_severity(node[key].as<std::string>());
    return s == Severity::None && node[key].as<std::string>() != "none" ? fallback : s;
}

template <typename T>
auto get_vector(const YAML::Node& node, const std::string& key,
                std::vector<T> fallback) -> std::vector<T> {
    if (!node || !node.IsMap() || !node[key] || !node[key].IsSequence()) {
        return fallback;
    }
    std::vector<T> out;
    out.reserve(node[key].size());
    for (const auto& el : node[key]) {
        try {
            out.push_back(el.as<T>());
        } catch (const YAML::Exception&) {
            // skip malformed
            (void)0;  // swallowed: best-effort
        }
    }
    return out;
}

void parse_project(const YAML::Node& node, ProjectConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.name = get_or<std::string>(node, "name", cfg.name);
    cfg.owner = get_or<std::string>(node, "owner", cfg.owner);
    cfg.languages = get_vector<std::string>(node, "languages", cfg.languages);
}

void parse_scan(const YAML::Node& node, ScanConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.mode = parse_scan_mode(get_or<std::string>(node, "mode", scan_mode_str(cfg.mode)));
    cfg.target = get_or<std::string>(node, "target", cfg.target);
    cfg.include = get_vector<std::string>(node, "include", cfg.include);
    cfg.exclude = get_vector<std::string>(node, "exclude", cfg.exclude);
    cfg.max_file_size_kb = get_or<int>(node, "max_file_size_kb", cfg.max_file_size_kb);
    cfg.follow_symlinks = get_or<bool>(node, "follow_symlinks", cfg.follow_symlinks);
    cfg.respect_gitignore = get_or<bool>(node, "respect_gitignore", cfg.respect_gitignore);
}

void parse_cache(const YAML::Node& node, CacheConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.enabled = get_or<bool>(node, "enabled", cfg.enabled);
    cfg.path = get_or<std::string>(node, "path", cfg.path);
    cfg.ttl_days = get_or<int>(node, "ttl_days", cfg.ttl_days);
}

void parse_rules(const YAML::Node& node, RulesConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.packs = get_vector<std::string>(node, "packs", cfg.packs);

    if (node["custom"] && node["custom"].IsSequence()) {
        cfg.custom.clear();
        for (const auto& cr : node["custom"]) {
            if (!cr.IsMap()) continue;
            CustomRule r;
            r.id        = get_or<std::string>(cr, "id", "");
            r.pattern   = get_or<std::string>(cr, "pattern", "");
            r.message   = get_or<std::string>(cr, "message", "");
            r.severity  = get_severity(cr, "severity", Severity::Medium);
            r.languages = get_vector<std::string>(cr, "languages", {});
            r.cwe       = get_or<std::string>(cr, "cwe", "");
            if (!r.id.empty() && !r.pattern.empty()) cfg.custom.push_back(r);
        }
    }

    if (node["severity_overrides"] && node["severity_overrides"].IsMap()) {
        cfg.severity_overrides.clear();
        for (const auto& kv : node["severity_overrides"]) {
            if (!kv.first.IsScalar() || !kv.second.IsScalar()) continue;
            cfg.severity_overrides[kv.first.as<std::string>()] =
                parse_severity(kv.second.as<std::string>());
        }
    }
}

void parse_baseline(const YAML::Node& node, BaselineConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.path = get_or<std::string>(node, "path", cfg.path);
    cfg.fail_on_new_only = get_or<bool>(node, "fail_on_new_only", cfg.fail_on_new_only);
}

void parse_gates(const YAML::Node& node, GatesConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.advisory_threshold = get_severity(node, "advisory_threshold", cfg.advisory_threshold);
    cfg.blocking_threshold = get_severity(node, "blocking_threshold", cfg.blocking_threshold);
    int secs = get_or<int>(node, "max_runtime_seconds",
                           static_cast<int>(cfg.max_runtime.count()));
    if (secs > 0) cfg.max_runtime = std::chrono::seconds{secs};
}

void parse_triage(const YAML::Node& node, TriageConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.enabled = get_or<bool>(node, "enabled", cfg.enabled);
    if (node["ensemble"] && node["ensemble"].IsMap()) {
        const auto& en = node["ensemble"];
        cfg.ensemble.providers = get_vector<std::string>(en, "providers", cfg.ensemble.providers);
        cfg.ensemble.quorum    = get_or<int>(en, "quorum", cfg.ensemble.quorum);
        cfg.ensemble.send_source_code =
            get_or<bool>(en, "send_source_code", cfg.ensemble.send_source_code);
    }
}

void parse_evidence(const YAML::Node& node, EvidenceConfig& cfg) {
    if (!node || !node.IsMap()) return;
    cfg.emit      = get_or<bool>(node, "emit",      cfg.emit);
    cfg.sarif     = get_or<bool>(node, "sarif",     cfg.sarif);
    cfg.cyclonedx = get_or<bool>(node, "cyclonedx", cfg.cyclonedx);
    cfg.spdx      = get_or<bool>(node, "spdx",      cfg.spdx);
    cfg.openvex   = get_or<bool>(node, "openvex",   cfg.openvex);
    cfg.rekor_upload = get_or<bool>(node, "rekor_upload", cfg.rekor_upload);

    std::string s = get_or<std::string>(node, "signing", "keyless-oidc");
    if      (s == "off")           cfg.signing = SigningMode::Off;
    else if (s == "key-backed")    cfg.signing = SigningMode::KeyBacked;
    else                           cfg.signing = SigningMode::KeylessOidc;

    cfg.frameworks = get_vector<std::string>(node, "frameworks", cfg.frameworks);
}

void parse_providers(const YAML::Node& node, ProviderRouting& cfg) {
    if (!node || !node.IsMap()) return;
    if (node["research"] && node["research"].IsScalar()) cfg.research = node["research"].as<std::string>();
    if (node["coding"]   && node["coding"].IsScalar())   cfg.coding   = node["coding"].as<std::string>();
    if (node["security"] && node["security"].IsScalar()) cfg.security = node["security"].as<std::string>();
}

auto expand_home(std::string path) -> std::string {
    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home != nullptr) path.replace(0, 1, home);
    }
    return path;
}

} // namespace

auto default_config() noexcept -> RuntimeConfig {
    return RuntimeConfig{};
}

auto parse_scan_mode(const std::string& s) noexcept -> ScanMode {
    std::string lower;
    lower.reserve(s.size());
    for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower == "triage")   return ScanMode::Triage;
    if (lower == "review")   return ScanMode::Review;
    if (lower == "forensic") return ScanMode::Forensic;
    return ScanMode::Check;
}

auto scan_mode_str(ScanMode m) noexcept -> const char* {
    switch (m) {
        case ScanMode::Triage:   return "triage";
        case ScanMode::Check:    return "check";
        case ScanMode::Review:   return "review";
        case ScanMode::Forensic: return "forensic";
    }
    return "check";
}

auto load_yaml_file(const std::filesystem::path& path)
    -> std::expected<RuntimeConfig, ConfigError> {
    namespace fs = std::filesystem;

    if (!fs::exists(path)) {
        return std::unexpected(ConfigError{
            .message = "Config file does not exist",
            .file = path,
        });
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(path.string());
    } catch (const YAML::Exception& e) {
        return std::unexpected(ConfigError{
            .message = std::string{"YAML parse error: "} + e.what(),
            .file = path,
            .line = static_cast<int>(e.mark.line + 1),
        });
    }

    if (!root || !root.IsMap()) {
        return std::unexpected(ConfigError{
            .message = "Top-level YAML node is not a map",
            .file = path,
        });
    }

    RuntimeConfig cfg = default_config();
    cfg.schema_version = get_or<std::string>(root, "version", "1");
    if (cfg.schema_version != "1") {
        return std::unexpected(ConfigError{
            .message = "Unsupported schema version: " + cfg.schema_version,
            .file = path,
        });
    }

    parse_project   (root["project"],   cfg.project);
    parse_scan      (root["scan"],      cfg.scan);
    parse_cache     (root["cache"],     cfg.cache);
    parse_rules     (root["rules"],     cfg.rules);
    parse_baseline  (root["baseline"],  cfg.baseline);
    parse_gates     (root["gates"],     cfg.gates);
    parse_triage    (root["triage"],    cfg.triage);
    parse_evidence  (root["evidence"],  cfg.evidence);
    parse_providers (root["providers"], cfg.providers);

    cfg.cache.path = expand_home(cfg.cache.path);
    cfg.sources.push_back(path);
    return cfg;
}

void apply_env_overrides(RuntimeConfig& config) noexcept {
    auto env = [](const char* k) -> std::optional<std::string> {
        const char* v = std::getenv(k);
        return v != nullptr ? std::optional<std::string>{v} : std::nullopt;
    };

    if (auto v = env("EUXIS_DEFAULT_RESEARCH_PROVIDER")) config.providers.research = *v;
    if (auto v = env("EUXIS_DEFAULT_CODING_PROVIDER"))   config.providers.coding   = *v;
    if (auto v = env("EUXIS_DEFAULT_SECURITY_PROVIDER")) config.providers.security = *v;

    if (auto v = env("EUXIS_SCAN_MODE")) config.scan.mode = parse_scan_mode(*v);
    if (auto v = env("EUXIS_CACHE_ENABLED")) {
        config.cache.enabled = (*v == "1" || *v == "true" || *v == "TRUE");
    }
    if (auto v = env("EUXIS_GATES_BLOCKING")) {
        auto s = euxis::security::parse_severity(*v);
        if (s != euxis::security::Severity::None || *v == "none") {
            config.gates.blocking_threshold = s;
        }
    }
}

void merge_into(RuntimeConfig& base, const RuntimeConfig& overlay) noexcept {
    const RuntimeConfig zero{};

    // Project
    if (overlay.project.name  != zero.project.name)  base.project.name  = overlay.project.name;
    if (overlay.project.owner != zero.project.owner) base.project.owner = overlay.project.owner;
    if (!overlay.project.languages.empty()) base.project.languages = overlay.project.languages;

    // Scan
    if (overlay.scan.mode   != zero.scan.mode)   base.scan.mode   = overlay.scan.mode;
    if (overlay.scan.target != zero.scan.target) base.scan.target = overlay.scan.target;
    if (!overlay.scan.include.empty()) base.scan.include = overlay.scan.include;
    if (overlay.scan.exclude != zero.scan.exclude) base.scan.exclude = overlay.scan.exclude;
    base.scan.max_file_size_kb  = overlay.scan.max_file_size_kb;
    base.scan.follow_symlinks   = overlay.scan.follow_symlinks;
    base.scan.respect_gitignore = overlay.scan.respect_gitignore;

    // Cache
    base.cache = overlay.cache;

    // Rules
    if (overlay.rules.packs != zero.rules.packs) base.rules.packs = overlay.rules.packs;
    if (!overlay.rules.custom.empty()) base.rules.custom = overlay.rules.custom;
    if (!overlay.rules.severity_overrides.empty()) {
        base.rules.severity_overrides = overlay.rules.severity_overrides;
    }

    base.baseline  = overlay.baseline;
    base.gates     = overlay.gates;
    base.triage    = overlay.triage;
    base.evidence  = overlay.evidence;
    if (overlay.providers.research) base.providers.research = overlay.providers.research;
    if (overlay.providers.coding)   base.providers.coding   = overlay.providers.coding;
    if (overlay.providers.security) base.providers.security = overlay.providers.security;

    for (const auto& s : overlay.sources) base.sources.push_back(s);
}

auto resolve_config(
    const std::filesystem::path& project_root,
    const std::filesystem::path& user_home)
    -> std::expected<RuntimeConfig, ConfigError> {
    namespace fs = std::filesystem;

    RuntimeConfig effective = default_config();

    fs::path home = user_home;
    if (home.empty()) {
        const char* h = std::getenv("HOME");
        if (h != nullptr) home = h;
    }
    if (!home.empty()) {
        fs::path user_file = home / ".euxis" / "config.yaml";
        if (fs::exists(user_file)) {
            auto loaded = load_yaml_file(user_file);
            if (loaded) merge_into(effective, *loaded);
            else return std::unexpected(loaded.error());
        }
    }

    if (!project_root.empty()) {
        for (const auto& name : {".euxis.yaml", ".euxis.yml"}) {
            fs::path proj = project_root / name;
            if (fs::exists(proj)) {
                auto loaded = load_yaml_file(proj);
                if (loaded) {
                    merge_into(effective, *loaded);
                    break;
                }
                return std::unexpected(loaded.error());
            }
        }
    }

    apply_env_overrides(effective);
    return effective;
}

} // namespace euxis::runtime
