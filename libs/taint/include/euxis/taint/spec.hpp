/// @file
/// @brief Source / sink / sanitizer specs for the taint analyzer.
///
/// A `TaintSpec` is a curated list of patterns describing where
/// untrusted data enters the program (sources), where it must not
/// arrive without escaping (sinks), and which call patterns
/// neutralise it (sanitizers). The analyzer in analyzer.hpp walks
/// the CPG's DDG edges from each source and reports flows to sinks
/// unless a sanitizer intervenes.
///
/// Reference: Sabelfeld & Myers, "Language-Based Information Flow
/// Security" (IEEE J-SAC 2003). Joern uses the same source/sink/
/// sanitizer triple as the basis for its taint queries; ours is the
/// C++23 realisation aligned with libs/cpg + libs/security.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "euxis/parse/types.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::taint {

/// One source pattern. A CPG node matches if:
///   1. Its language is in `languages` (or the spec is universal).
///   2. The pattern text appears as a substring of the node's
///      source bytes (Ast::node_text(node.range)).
///
/// Week 13 keeps matching deliberately simple — literal substring.
/// Richer matchers (regex, CPG-shape queries) plug into the same
/// type when the rule corpus drives the requirement.
struct SourceSpec {
    /// Stable identifier surfaced in TaintFlow.source_spec_id.
    std::string id;

    /// Languages the spec applies to. Empty = universal.
    std::vector<euxis::parse::Language> languages;

    /// Literal pattern to match against node source bytes.
    std::string pattern;

    /// Human-readable description; surfaced in derived Findings.
    std::string description;
};

/// One sink pattern. Same matching semantics as SourceSpec.
/// Optionally carries CWE / OWASP metadata so flows ending at this
/// sink can produce a Finding with the right taxa attached.
struct SinkSpec {
    std::string id;
    std::vector<euxis::parse::Language> languages;
    std::string pattern;
    std::string description;

    std::optional<std::string> cwe;                      ///< e.g. "CWE-89"
    std::optional<euxis::security::OwaspCategory> owasp; ///< e.g. A03:2025
    euxis::security::Severity severity{euxis::security::Severity::High};
};

/// One sanitizer pattern. When the BFS walker hits a node matching
/// this spec, propagation through that node is terminated — the
/// flow is considered safe past that point.
struct SanitizerSpec {
    std::string id;
    std::vector<euxis::parse::Language> languages;
    std::string pattern;
    std::string description;
};

/// Bundle of specs the analyzer consumes.
struct TaintSpec {
    std::string name{"anonymous"};
    std::vector<SourceSpec> sources;
    std::vector<SinkSpec> sinks;
    std::vector<SanitizerSpec> sanitizers;
};

/// Whether a spec applies to the given language. Empty languages
/// vector counts as universal.
[[nodiscard]] auto applies_to(const std::vector<euxis::parse::Language>& langs,
                              euxis::parse::Language lang) noexcept -> bool;

} // namespace euxis::taint
