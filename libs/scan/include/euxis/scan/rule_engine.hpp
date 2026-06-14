/// @file
/// @brief Apply a RulePack against a CPG, emit canonical Findings.
///
/// The Week 11 engine does literal substring matching over each
/// CPG node's source bytes. For each matching node it produces one
/// `euxis::security::Finding` whose:
///
///   - rule_id           = "opengrep/<rulepack-name>/<rule-id>"
///   - message           = rule.message
///   - severity          = mapped from RuleSeverity
///   - confidence        = Probable
///   - source            = euxis::security::RuleSource::OpenGrep
///   - primary_location  = the matched node's SourceRange
///   - cwe               = rule.metadata.primary_cwe
///   - owasp             = rule.metadata.primary_owasp
///   - compliance_taxa   = metadata.references
///   - stable_fingerprint = FNV-1a of (rule_id, file, ecosystem, name)
///
/// The engine deliberately does NOT yet:
///
///   - Constrain matches to specific NodeKinds (e.g. only `Call`).
///   - Resolve metavariables across Composite patterns.
///   - Honour `pattern-not` / `pattern-inside`.
///
/// All three slot in via the Pattern type's existing children
/// vector when the corresponding constructs land.
#pragma once

#include <vector>

#include "euxis/cpg/graph.hpp"
#include "euxis/parse/ast.hpp"
#include "euxis/scan/rule.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::scan {

struct EngineStats {
    std::size_t rules_evaluated{0};
    std::size_t nodes_visited{0};
    std::size_t findings_emitted{0};
    std::size_t skipped_language_mismatch{0};
};

struct EngineResult {
    std::vector<euxis::security::Finding> findings;
    EngineStats stats;
};

/// Apply every rule in `pack` against the (Ast, Graph) pair. The
/// `file` argument is recorded in each Finding's primary_location
/// so downstream emitters can render it.
[[nodiscard]] auto apply_rules(const RulePack& pack,
                               const euxis::parse::Ast& ast,
                               const euxis::cpg::Graph& graph,
                               const std::string& file = "<unknown>")
    -> EngineResult;

} // namespace euxis::scan
