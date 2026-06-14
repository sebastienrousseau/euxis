/// @file
/// @brief OpenGrep / Semgrep-style rule representation.
///
/// `Rule` is the typed in-memory form of one entry in an OpenGrep
/// YAML rule pack. The loader (rule_loader.hpp) parses YAML into a
/// `RulePack`; the engine (rule_engine.hpp) applies a RulePack
/// against a CPG and emits canonical Findings.
///
/// Reference: https://semgrep.dev/docs/writing-rules/rule-syntax
/// (OpenGrep is the 10-vendor consortium LGPL-2.1 fork of Semgrep CE;
/// rule syntax is binary-compatible at the YAML layer.)
///
/// What the Week 11 engine supports
/// --------------------------------
///   - `id`              required
///   - `message`         required, human-readable
///   - `severity`        INFO / WARNING / ERROR -> euxis Severity
///   - `languages`       list of language tokens, mapped to our enum
///   - `pattern`         single literal substring (MVP scope)
///   - `patterns`        all child patterns must match (AND)
///   - `pattern-either`  at least one child pattern must match (OR)
///   - `metadata.cwe`    string or list -> euxis::security::CweRef
///   - `metadata.owasp`  string or list -> euxis::security::OwaspCategory
///   - `metadata.references` list of URLs
///
/// What this batch deliberately defers
/// -----------------------------------
///   - `pattern-not`               needs negation
///   - `pattern-inside`            needs scoping
///   - `metavariable-pattern`      needs metavariable binding
///   - `metavariable-regex`        ditto + Vectorscan
///   - Real AST-based pattern matching (tree-sitter S-expression
///     queries). Lands when the security-rule corpus drives the
///     requirement — for the v0.0.3 surface a literal substring
///     matcher catches the highest-value rules without committing
///     us to a particular query language yet.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "euxis/parse/types.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::scan {

/// Internal severity vocabulary mirroring OpenGrep YAML. The
/// engine maps this onto `euxis::security::Severity` when emitting
/// Findings.
enum class RuleSeverity {
    Info,
    Warning,
    Error,
};

/// Convert YAML severity strings ("INFO", "WARNING", "ERROR" — and
/// lower-case variants for forgiving rule writers) into the enum.
/// Returns `std::nullopt` for unknown input so the loader can
/// surface a parse error.
[[nodiscard]] auto parse_severity(std::string_view s) noexcept
    -> std::optional<RuleSeverity>;

/// Map RuleSeverity onto euxis::security::Severity for Finding
/// emission. RuleSeverity is intentionally smaller than the canonical
/// type so we don't impose Semgrep's vocabulary on the rest of the
/// pipeline.
[[nodiscard]] auto to_security_severity(RuleSeverity s) noexcept
    -> euxis::security::Severity;

/// Pattern node kinds the engine understands today.
enum class PatternKind {
    Literal,       ///< `pattern: foo`  — substring match
    Composite,     ///< `patterns: [...]`  — AND
    Alternation,   ///< `pattern-either: [...]`  — OR
};

/// One node in a rule's pattern tree.
struct Pattern {
    PatternKind kind{PatternKind::Literal};

    /// For Literal kind: the source-text substring to look for.
    /// Stored verbatim; the engine does literal byte comparison
    /// against `Ast::node_text(node.range)`.
    std::string text;

    /// For Composite and Alternation kinds: the operand patterns.
    std::vector<Pattern> children;
};

/// Metadata block, supporting both string and list forms in YAML.
struct RuleMetadata {
    /// First listed CWE (if any). The MVP normalises lists to the
    /// head element so the resulting Finding carries a single
    /// CweRef.
    std::optional<std::string> primary_cwe;

    /// First listed OWASP category. Parsed strings like "A03:2025"
    /// are translated to `euxis::security::OwaspCategory`.
    std::optional<euxis::security::OwaspCategory> primary_owasp;

    /// Free-form URLs surfaced in the Finding's compliance_taxa.
    std::vector<std::string> references;

    /// Any other top-level YAML scalars under metadata. Stored
    /// verbatim so the rule-pack author can pass through fields
    /// the engine doesn't model yet (FedRAMP control IDs,
    /// internal squad ownership, …).
    std::vector<std::pair<std::string, std::string>> extras;
};

/// One rule.
struct Rule {
    std::string id;        ///< required, unique within a pack
    std::string message;   ///< required, surfaced verbatim in Finding.message
    RuleSeverity severity{RuleSeverity::Warning};

    /// Languages the rule targets. Empty means "applies to every
    /// language" — useful for generic text matchers.
    std::vector<euxis::parse::Language> languages;

    Pattern pattern;
    RuleMetadata metadata;
};

/// A set of rules loaded from one YAML document. The pack-level
/// `name` is the document's filename (or "anonymous" for in-memory
/// rule packs); it surfaces in the engine's diagnostics.
struct RulePack {
    std::string name{"anonymous"};
    std::vector<Rule> rules;
};

/// Parse the language token vocabulary OpenGrep uses (`c`, `cpp`,
/// `rust`, `go`, `python`, `js`, `ts`, `java`, …). Returns the
/// matching `euxis::parse::Language` or `std::nullopt`.
[[nodiscard]] auto parse_language_token(std::string_view s) noexcept
    -> std::optional<euxis::parse::Language>;

/// Translate an OWASP token like "A03:2025" or "A03" into the
/// canonical enum. Year-less tokens default to the 2025 mapping.
[[nodiscard]] auto parse_owasp_token(std::string_view s) noexcept
    -> std::optional<euxis::security::OwaspCategory>;

} // namespace euxis::scan
