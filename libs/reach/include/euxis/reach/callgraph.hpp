/// @file
/// @brief Intra-file call graph built from a libs/cpg Graph.
///
/// For every `Call` node in the CPG, the builder looks at the
/// call's source-text identifier and pairs it with any
/// `FunctionDef` whose `name` (or whose source text in the
/// absence of an explicit name) contains that identifier. The
/// result is a `CallGraph` of (caller_function, callee_function)
/// pairs.
///
/// Limitations the Week 16 builder commits to
/// ------------------------------------------
///   - Intra-file only. Cross-file resolution needs a module-
///     wide symbol index; deferred until the per-project scanner
///     lifts findings out of the per-file loop.
///   - Name-based, not type-based. `foo()` in C++ matches every
///     `FunctionDef` whose declarator includes `foo`, including
///     overloads and unrelated free functions. The taint and
///     reachability consumers are over-approximate — false
///     positives, not false negatives.
///   - No virtual dispatch resolution. Calls through pointers,
///     vtables, function objects, etc. are not modelled.
///
/// These approximations are correct (no missed real edges) at
/// the cost of including extra edges that don't really exist.
/// The reachability consumer in reachability.hpp treats a
/// finding as "reachable" if ANY chain reaches it, which is the
/// right direction given the soundness profile.
#pragma once

#include <vector>

#include "euxis/cpg/graph.hpp"
#include "euxis/parse/ast.hpp"

namespace euxis::reach {

/// One directed edge: `caller` calls `callee`. Both ids reference
/// the FunctionDef nodes in the CPG, NOT the Call site itself,
/// because the reachability BFS operates on function granularity.
struct CallEdge {
    euxis::cpg::NodeId caller{};
    euxis::cpg::NodeId callee{};
};

struct CallGraph {
    std::vector<CallEdge> edges;
};

struct CallGraphStats {
    std::size_t function_count{0};
    std::size_t call_sites{0};
    std::size_t resolved_calls{0};
    std::size_t unresolved_calls{0};
};

struct CallGraphResult {
    CallGraph graph;
    CallGraphStats stats;
};

/// Build a CallGraph from a parsed CPG + its source Ast. The Ast
/// is needed for `node_text()` lookups when classifying calls.
[[nodiscard]] auto build_call_graph(const euxis::cpg::Graph& graph,
                                     const euxis::parse::Ast& ast)
    -> CallGraphResult;

/// Walk parent (AstChild) edges upward from `node` to find the
/// enclosing FunctionDef. Returns `kNullNode` when the node is at
/// the translation-unit level (e.g. a top-level statement).
[[nodiscard]] auto enclosing_function(const euxis::cpg::Graph& graph,
                                       euxis::cpg::NodeId node) noexcept
    -> euxis::cpg::NodeId;

} // namespace euxis::reach
