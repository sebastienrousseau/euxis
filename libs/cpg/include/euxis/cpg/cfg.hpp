/// @file
/// @brief Intra-procedural Control Flow Graph construction.
///
/// `build_cfg` mutates a Graph that already carries the AST
/// projection (from `cpg::build`) by adding `EdgeKind::Cfg` edges
/// between statement-shaped nodes. CFG construction is purely
/// additive — no AstChild edge is touched, no node is added.
///
/// What the Week 9 builder produces, per FunctionDef:
///
///   1. Sequential edges between consecutive children of every
///      Block descendant.
///
///         stmt1 ──Cfg──▶ stmt2 ──Cfg──▶ stmt3
///
///   2. Branch-entry edges from an If node into the first AstChild
///      Block of each branch (the "then" and, when present, the
///      "else" body).
///
///         If ──Cfg──▶ then-block-first-stmt
///         If ──Cfg──▶ else-block-first-stmt
///
///   3. Loop back-edges from the last statement of a Loop's body
///      Block to the Loop node, modelling the iteration.
///
///         body-last-stmt ──Cfg──▶ Loop
///         Loop ──Cfg──▶ body-first-stmt
///
/// What this batch deliberately does NOT model (lands later):
///
///   - break / continue resolution to enclosing loop exits
///   - switch / match dispatch edges
///   - exception flow (try/catch/throw)
///   - explicit return targeting a synthetic function exit node
///   - goto resolution (rare in modern source; deferred)
///   - inter-procedural Call edges (P1.5)
///
/// All of those follow-ups add edges of EdgeKind::Cfg or
/// EdgeKind::Call without touching the public surface here.
#pragma once

#include <cstddef>

#include "euxis/cpg/graph.hpp"

namespace euxis::cpg {

struct CfgBuildOptions {
    /// When true, emit branch-entry edges from an If node to its
    /// then-block and, if present, its else-block. Default: on.
    bool emit_if_branch_edges{true};

    /// When true, emit back-edges from the last statement of a
    /// Loop's body to the Loop node itself. Default: on.
    bool emit_loop_back_edges{true};
};

/// Diagnostics returned by `build_cfg`. Callers feed these into
/// metrics / bencher.dev / the architecture-quality gate.
struct CfgStats {
    std::size_t functions_processed{0};
    std::size_t sequential_edges{0};
    std::size_t branch_entry_edges{0};
    std::size_t loop_back_edges{0};
};

/// Add EdgeKind::Cfg edges to `graph`. The graph must already
/// contain the AST projection (from `cpg::build`). Returns
/// per-call statistics so the caller can log / compare across
/// runs.
auto build_cfg(Graph& graph, const CfgBuildOptions& opts = {})
    -> CfgStats;

} // namespace euxis::cpg
