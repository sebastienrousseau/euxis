#include "euxis/scan/rule_loader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <yaml-cpp/yaml.h>

namespace euxis::scan {

namespace {

/// Extract a scalar field; returns the YAML value as std::string
/// or an empty string when the field is missing or non-scalar.
auto scalar_or_empty(const YAML::Node& node, const std::string& key)
    -> std::string {
    if (!node || !node.IsMap() || !node[key]) return {};
    try {
        return node[key].as<std::string>();
    } catch (...) {
        return {};
    }
}

/// Build a Pattern from one YAML node. Supports either a bare
/// scalar (literal), or a mapping with `pattern` / `patterns` /
/// `pattern-either` keys.
auto build_pattern_from_node(const YAML::Node& node) -> Pattern;

/// Collect a Composite/Alternation operand list from a sequence
/// node. Each entry may be a scalar (treated as literal) or a
/// mapping that nests further patterns.
auto build_pattern_list(const YAML::Node& seq) -> std::vector<Pattern> {
    std::vector<Pattern> out;
    if (!seq || !seq.IsSequence()) return out;
    out.reserve(seq.size());
    for (const auto& entry : seq) {
        out.push_back(build_pattern_from_node(entry));
    }
    return out;
}

/// Wrap a child pattern as a single-element child list under the
/// given kind. Used by `pattern-not` and `pattern-inside`.
auto wrap_unary(PatternKind kind, const YAML::Node& child_node) -> Pattern {
    Pattern p;
    p.kind = kind;
    p.children.push_back(build_pattern_from_node(child_node));
    return p;
}

auto build_pattern_from_node(const YAML::Node& node) -> Pattern {
    Pattern p;
    if (!node) return p;

    if (node.IsScalar()) {
        p.kind = PatternKind::Literal;
        try { p.text = node.as<std::string>(); } catch (...) {}
        return p;
    }

    if (!node.IsMap()) return p;

    // Negation: `pattern-not: <child>`. Child may be a scalar
    // (treated as literal) or a nested map.
    if (node["pattern-not"]) {
        return wrap_unary(PatternKind::Negation, node["pattern-not"]);
    }
    // Scoping: `pattern-inside: <child>`. Same child shape.
    if (node["pattern-inside"]) {
        return wrap_unary(PatternKind::Inside, node["pattern-inside"]);
    }
    // Metavariable refinement keys are recognised but not yet
    // evaluated by the engine. Surface a one-time warning so rule
    // authors see that the field had no effect rather than
    // wondering why their rule never matched.
    if (node["metavariable-pattern"] || node["metavariable-regex"]) {
        std::cerr << "euxis/scan: metavariable-pattern / "
                     "metavariable-regex recognised but not yet "
                     "evaluated by the engine — see rule.hpp\n";
    }

    if (node["pattern-either"] && node["pattern-either"].IsSequence()) {
        p.kind     = PatternKind::Alternation;
        p.children = build_pattern_list(node["pattern-either"]);
        return p;
    }
    if (node["patterns"] && node["patterns"].IsSequence()) {
        p.kind     = PatternKind::Composite;
        p.children = build_pattern_list(node["patterns"]);
        return p;
    }
    if (node["pattern"]) {
        p.kind = PatternKind::Literal;
        try {
            p.text = node["pattern"].as<std::string>();
        } catch (...) {}
        return p;
    }
    return p;
}

/// Parse the top-level `pattern` / `patterns` / `pattern-either` /
/// `pattern-not` / `pattern-inside` block of a Rule. Any of those
/// is accepted at the rule level; the engine evaluates whichever
/// kind survives parsing.
auto build_rule_pattern(const YAML::Node& rule_node) -> Pattern {
    Pattern p;
    if (!rule_node || !rule_node.IsMap()) return p;
    if (rule_node["pattern-either"] && rule_node["pattern-either"].IsSequence()) {
        p.kind     = PatternKind::Alternation;
        p.children = build_pattern_list(rule_node["pattern-either"]);
        return p;
    }
    if (rule_node["patterns"] && rule_node["patterns"].IsSequence()) {
        p.kind     = PatternKind::Composite;
        p.children = build_pattern_list(rule_node["patterns"]);
        return p;
    }
    if (rule_node["pattern-not"]) {
        return wrap_unary(PatternKind::Negation, rule_node["pattern-not"]);
    }
    if (rule_node["pattern-inside"]) {
        return wrap_unary(PatternKind::Inside, rule_node["pattern-inside"]);
    }
    if (rule_node["pattern"]) {
        p.kind = PatternKind::Literal;
        try {
            p.text = rule_node["pattern"].as<std::string>();
        } catch (...) {}
        return p;
    }
    return p;
}

/// Pull `cwe` from metadata, accepting either a scalar or a list.
auto extract_first_cwe(const YAML::Node& metadata) -> std::optional<std::string> {
    if (!metadata || !metadata.IsMap()) return std::nullopt;
    auto cwe_node = metadata["cwe"];
    if (!cwe_node) return std::nullopt;
    try {
        if (cwe_node.IsScalar()) return cwe_node.as<std::string>();
        if (cwe_node.IsSequence() && cwe_node.size() > 0) {
            return cwe_node[0].as<std::string>();
        }
    } catch (...) {}
    return std::nullopt;
}

/// Pull `owasp` from metadata, accepting either a scalar or a list.
auto extract_first_owasp(const YAML::Node& metadata)
    -> std::optional<euxis::security::OwaspCategory> {
    if (!metadata || !metadata.IsMap()) return std::nullopt;
    auto owasp_node = metadata["owasp"];
    if (!owasp_node) return std::nullopt;
    try {
        if (owasp_node.IsScalar()) {
            return parse_owasp_token(owasp_node.as<std::string>());
        }
        if (owasp_node.IsSequence() && owasp_node.size() > 0) {
            return parse_owasp_token(owasp_node[0].as<std::string>());
        }
    } catch (...) {}
    return std::nullopt;
}

auto extract_references(const YAML::Node& metadata) -> std::vector<std::string> {
    std::vector<std::string> out;
    if (!metadata || !metadata.IsMap()) return out;
    auto refs = metadata["references"];
    if (!refs) return out;
    try {
        if (refs.IsScalar()) {
            out.push_back(refs.as<std::string>());
        } else if (refs.IsSequence()) {
            for (const auto& r : refs) {
                if (r.IsScalar()) out.push_back(r.as<std::string>());
            }
        }
    } catch (...) {}
    return out;
}

auto extract_languages(const YAML::Node& rule_node)
    -> std::vector<euxis::parse::Language> {
    std::vector<euxis::parse::Language> out;
    auto langs = rule_node["languages"];
    if (!langs || !langs.IsSequence()) return out;
    for (const auto& l : langs) {
        if (!l.IsScalar()) continue;
        try {
            auto resolved = parse_language_token(l.as<std::string>());
            if (resolved) out.push_back(*resolved);
        } catch (...) {}
    }
    return out;
}

auto build_rule(const YAML::Node& rule_node)
    -> std::expected<Rule, std::string> {
    if (!rule_node || !rule_node.IsMap()) {
        return std::unexpected("rule entry is not a YAML map");
    }
    Rule r;
    r.id      = scalar_or_empty(rule_node, "id");
    r.message = scalar_or_empty(rule_node, "message");
    if (r.id.empty()) {
        return std::unexpected("rule missing required `id`");
    }
    if (r.message.empty()) {
        return std::unexpected("rule '" + r.id + "' missing required `message`");
    }

    auto sev = parse_severity(scalar_or_empty(rule_node, "severity"));
    r.severity = sev.value_or(RuleSeverity::Warning);

    r.languages = extract_languages(rule_node);
    r.pattern   = build_rule_pattern(rule_node);

    auto metadata = rule_node["metadata"];
    r.metadata.primary_cwe   = extract_first_cwe(metadata);
    r.metadata.primary_owasp = extract_first_owasp(metadata);
    r.metadata.references    = extract_references(metadata);

    return r;
}

} // namespace

auto load_rules_yaml(std::string_view yaml, std::string name)
    -> std::expected<RulePack, LoadError> {
    RulePack pack;
    pack.name = std::move(name);

    YAML::Node root;
    try {
        root = YAML::Load(std::string{yaml});
    } catch (const YAML::Exception& e) {
        return std::unexpected(LoadError{
            .message = std::string{"YAML parse error: "} + e.what(),
            .file    = {},
            .line    = static_cast<int>(e.mark.line + 1),
        });
    }

    if (!root || !root.IsMap()) {
        return std::unexpected(LoadError{
            .message = "rule pack root is not a YAML mapping",
            .file    = {},
        });
    }
    auto rules_node = root["rules"];
    if (!rules_node || !rules_node.IsSequence()) {
        return std::unexpected(LoadError{
            .message = "rule pack missing top-level `rules:` sequence",
            .file    = {},
        });
    }

    pack.rules.reserve(rules_node.size());
    int idx = 0;
    for (const auto& entry : rules_node) {
        auto built = build_rule(entry);
        if (!built) {
            return std::unexpected(LoadError{
                .message = "rules[" + std::to_string(idx) + "]: " + built.error(),
                .file    = {},
            });
        }
        pack.rules.push_back(std::move(*built));
        ++idx;
    }
    return pack;
}

auto load_rules_file(const std::filesystem::path& path)
    -> std::expected<RulePack, LoadError> {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected(LoadError{
            .message = "cannot open rule file",
            .file    = path,
        });
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    auto loaded = load_rules_yaml(ss.str(), path.filename().string());
    if (!loaded) {
        auto err = loaded.error();
        err.file = path;
        return std::unexpected(std::move(err));
    }
    return loaded;
}

} // namespace euxis::scan
