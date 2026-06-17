/// @file
/// @brief Forward call-edge BFS from entry-point functions.
///
/// Given a CallGraph and an entry-point policy, `compute_reachable`
/// returns the set of FunctionDef ids transitively reachable. The
/// `annotate_findings` helper then maps that set onto a list of
/// canonical Findings — every Finding whose location sits inside
/// a reachable function gets `euxis:reachable:true` added to its
/// `compliance_taxa`; findings in unreachable functions get
/// `euxis:reachable:false`; findings outside any function (top-
/// level module code, includes, comments) get
/// `euxis:reachable:unknown`.
///
/// The Endor Labs reachability pitch (97 % SCA noise reduction)
/// generalises to first-party findings too — a SQL-injection
/// finding in a dead method is, by definition, not exploitable.
/// Our implementation is intentionally conservative: by default
/// every top-level FunctionDef is an entry point (because we
/// can't tell a library export from an internal helper), so the
/// reachability flag is a strong "yes" but a weak "no" — false
/// negatives, not false positives.
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "euxis/cpg/graph.hpp"
#include "euxis/parse/ast.hpp"
#include "euxis/reach/callgraph.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::reach {

struct ReachabilityConfig {
    /// Function names that mark explicit entry points. Empty by
    /// default — combined with `top_level_functions_are_entries`
    /// gives sensible coverage for libraries (every top-level
    /// function is an entry) and standalone programs (main is an
    /// entry by virtue of being top-level).
    std::vector<std::string> entry_function_names;

    /// When true, every FunctionDef that has no AstChild
    /// FunctionDef ancestor is treated as an entry. Default: on.
    /// Set to false when the consumer can provide a precise entry
    /// list (e.g. an HTTP handler registry).
    bool top_level_functions_are_entries{true};

    /// Maximum BFS depth. 0 means unlimited. Default 0 — call
    /// graphs are flat enough that depth limits aren't typically
    /// needed; the knob exists for catastrophic-codegen cases.
    std::size_t max_depth{0};
};

struct ReachabilityResult {
    /// Function ids transitively reachable from the entry-point
    /// set.
    std::unordered_set<std::uint32_t> reachable_functions;

    /// How many entry points seeded the BFS.
    std::size_t entry_points{0};
};

/// Compute the set of reachable functions per the config.
[[nodiscard]] auto compute_reachable(const euxis::cpg::Graph& graph,
                                      const euxis::parse::Ast& ast,
                                      const CallGraph& cg,
                                      const ReachabilityConfig& config = {})
    -> ReachabilityResult;

/// For each finding in `findings`, append one entry to its
/// `compliance_taxa` vector:
///   `euxis:reachable:true`     — finding's enclosing function is reachable
///   `euxis:reachable:false`    — enclosing function is NOT reachable
///   `euxis:reachable:unknown`  — finding has no enclosing function
///
/// The annotation is purely additive — no existing taxa are
/// removed and the Finding's severity / confidence / fingerprint
/// are not touched. Consumers that want to gate on reachability
/// filter on the taxa string after the fact.
void annotate_findings(const euxis::cpg::Graph& graph,
                       const ReachabilityResult& reach,
                       const std::vector<euxis::security::Finding>::iterator begin,
                       const std::vector<euxis::security::Finding>::iterator end);

} // namespace euxis::reach
