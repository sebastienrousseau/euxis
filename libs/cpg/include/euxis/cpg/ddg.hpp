/// @file
/// @brief Intra-procedural Data Dependency Graph construction.
///
/// `build_ddg` mutates a Graph that already carries the AST
/// projection (cpg::build) and ideally the CFG (cpg::build_cfg) by
/// adding `EdgeKind::Ddg` edges from each definition of a name to
/// every subsequent use of that name within the same FunctionDef
/// body.
///
/// The Week 10 algorithm is a pre-order walk over each FunctionDef
/// body maintaining a name → last-def map:
///
///   1. Identifier nodes encountered in a "use" position emit
///      a Ddg edge from `def_map[name]` to the Identifier.
///   2. Declarations and the LHS of Assignments update
///      `def_map[name]` to point at the defining node.
///   3. Assignment children are processed RHS-first so the LHS
///      identifier itself does not become a use of the prior def.
///
/// This is intentionally a "reaching-defs-on-the-source-order"
/// approximation; it does NOT compute the canonical reaching-
/// definitions fixed-point over the CFG. What it gets right:
///
///   - Straight-line code: every use points at the prior def.
///   - Reassignment: subsequent uses point at the latest def.
///   - Compound expressions: nested identifier uses are walked
///     before LHS update.
///
/// What it misses (lands in follow-up batches):
///
///   - Branch join points. A use after an if-else whose both
///     branches re-define the same name will only see one of the
///     two defs (whichever the walker visited last).
///   - Loop fixed-point. Defs inside a loop body that flow back
///     across the back-edge are not surfaced as reaching their
///     own uses on later iterations.
///   - Aliasing (pointers, references, captures). Out of scope
///     until the P1.5 taint engine drives the requirement.
///
/// The taint engine in P1.5 can call `build_ddg` once and then
/// supplement the approximate edges with its own fixed-point on
/// the hot paths.
#pragma once

#include <cstddef>

#include "euxis/cpg/graph.hpp"

namespace euxis::cpg {

struct DdgBuildOptions {
    /// When true, definitions made by `Declaration` nodes are
    /// recorded in the def map. Default: on.
    bool track_declarations{true};

    /// When true, definitions made by `Assignment` nodes are
    /// recorded in the def map. Default: on.
    bool track_assignments{true};

    /// Maximum name-map size per FunctionDef. Guards against
    /// pathological generated code. Default: 4096 — well above
    /// any reasonable hand-written function.
    std::size_t max_names_per_function{4096};
};

struct DdgStats {
    std::size_t functions_processed{0};
    std::size_t definitions_recorded{0};
    std::size_t ddg_edges{0};
    std::size_t names_capped{0};
};

/// Add EdgeKind::Ddg edges to `graph`. The graph must already
/// contain the AST projection. CFG edges are not required for
/// the Week 10 approximation but will be consumed by the
/// fixed-point follow-up.
auto build_ddg(Graph& graph, const DdgBuildOptions& opts = {})
    -> DdgStats;

} // namespace euxis::cpg
