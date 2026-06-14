/// @file
/// @brief Forward-DDG BFS taint analyzer.
///
/// `analyze(graph, ast, spec)` walks every Ddg edge originating at
/// a node matching some `SourceSpec`, treats every Ddg target as a
/// candidate next hop, and reports a `TaintFlow` whenever the BFS
/// reaches a node matching some `SinkSpec`. Nodes matching a
/// `SanitizerSpec` terminate propagation through them.
///
/// What the Week 13 analyzer commits to
/// ------------------------------------
///   - Forward Ddg traversal only. CFG (statement successor) edges
///     are NOT walked; the def-use chain is sufficient for the
///     "did this value reach this sink" question.
///   - Function-local. The BFS doesn't follow Call edges (Call
///     edges are P1.7's domain). A flow that crosses a function
///     boundary is not reported until inter-procedural call
///     resolution lands.
///   - Multiple-path BFS — every reachable sink is emitted. We
///     don't try to deduplicate "the same flow via different
///     paths"; that's a UI concern for the next batch.
///   - O(N×E) per source — fine for the file-sized graphs euxis
///     scans. Bigger projects get the cache (libs/cache) so we
///     don't re-run the analyzer per file on every invocation.
///
/// Future batches
/// --------------
///   - Inter-procedural Call edges (P1.7 reachability pruner).
///   - Path-sensitive sanitizer interception (currently sanitizers
///     terminate any path through their node, regardless of
///     branch).
///   - Multi-LLM ensemble verification of high-severity flows
///     (P1.6 in the IP doc).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "euxis/cpg/graph.hpp"
#include "euxis/parse/ast.hpp"
#include "euxis/security/finding.hpp"
#include "euxis/taint/spec.hpp"

namespace euxis::taint {

/// One source→sink flow.
struct TaintFlow {
    euxis::cpg::NodeId source_node{};
    euxis::cpg::NodeId sink_node{};
    std::vector<euxis::cpg::NodeId> path;  ///< Source first, sink last
    std::string source_spec_id;
    std::string sink_spec_id;
};

/// Run statistics surfaced for diagnostics + the architecture-
/// quality gate.
struct AnalysisStats {
    std::size_t sources_matched{0};
    std::size_t sinks_matched{0};
    std::size_t sanitizers_matched{0};
    std::size_t flows_emitted{0};
    std::size_t bfs_nodes_visited{0};
};

struct AnalysisResult {
    std::vector<TaintFlow> flows;
    AnalysisStats stats;
};

/// Analyze a CPG. Returns every (source, sink) flow plus
/// aggregate counts. The graph should already carry the DDG
/// edges produced by `cpg::build_ddg`.
[[nodiscard]] auto analyze(const euxis::cpg::Graph& graph,
                            const euxis::parse::Ast& ast,
                            const TaintSpec& spec) -> AnalysisResult;

/// Convert each flow in `result` into a canonical
/// `euxis::security::Finding`. The Finding carries:
///   - rule_id            "euxis-taint/<source-id>->.<sink-id>"
///   - severity           from the matched SinkSpec
///   - confidence         Probable (single-path; ensemble lift is P1.6)
///   - source             RuleSource::LlmInferred placeholder until
///                        a dedicated taint enum lands
///   - cwe / owasp        from the matched SinkSpec
///   - primary_location   the sink node's SourceRange
///   - related_locations  source node + every intermediate Ddg hop
///   - stable_fingerprint FNV-1a over (rule_id, file, source raw_kind,
///                        sink raw_kind, sink line)
[[nodiscard]] auto flows_to_findings(
    const AnalysisResult& result,
    const euxis::cpg::Graph& graph,
    const std::string& file_path)
    -> std::vector<euxis::security::Finding>;

} // namespace euxis::taint
