#include "euxis/taint/analyzer.hpp"

#include <array>
#include <cstdint>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace euxis::taint {

namespace {

using euxis::cpg::EdgeKind;
using euxis::cpg::Graph;
using euxis::cpg::Node;
using euxis::cpg::NodeId;
using euxis::cpg::kNullNode;

/// Classify a node against the spec lists. Each node carries at
/// most one role; sanitizers take precedence over sinks which take
/// precedence over sources. This matches the semantics callers
/// expect — a function that both reads and quotes input is a
/// sanitizer, not a source.
enum class Role : std::uint8_t { None, Source, Sink, Sanitizer };

struct ClassificationEntry {
    Role role{Role::None};
    std::size_t spec_index{0};  ///< Index into the matching spec list
};

bool matches(const std::string& pattern, std::string_view text) noexcept {
    if (pattern.empty() || text.empty()) return false;
    return text.find(pattern) != std::string_view::npos;
}

std::vector<ClassificationEntry> classify_all(
    const Graph& graph,
    const euxis::parse::Ast& ast,
    const TaintSpec& spec,
    AnalysisStats& stats) {

    std::vector<ClassificationEntry> roles(graph.node_count() + 1U);
    const auto language = ast.language();

    for (const auto& node : graph.nodes()) {
        auto text = ast.node_text(node.range);
        if (text.empty()) continue;

        // Sanitizer wins.
        for (std::size_t i = 0; i < spec.sanitizers.size(); ++i) {
            const auto& s = spec.sanitizers[i];
            if (!applies_to(s.languages, language)) continue;
            if (!matches(s.pattern, text)) continue;
            roles[node.id.value] = {Role::Sanitizer, i};
            ++stats.sanitizers_matched;
            goto next_node;
        }
        // Sinks next.
        for (std::size_t i = 0; i < spec.sinks.size(); ++i) {
            const auto& s = spec.sinks[i];
            if (!applies_to(s.languages, language)) continue;
            if (!matches(s.pattern, text)) continue;
            roles[node.id.value] = {Role::Sink, i};
            ++stats.sinks_matched;
            goto next_node;
        }
        // Sources last.
        for (std::size_t i = 0; i < spec.sources.size(); ++i) {
            const auto& s = spec.sources[i];
            if (!applies_to(s.languages, language)) continue;
            if (!matches(s.pattern, text)) continue;
            roles[node.id.value] = {Role::Source, i};
            ++stats.sources_matched;
            break;
        }
        next_node:;
    }
    return roles;
}

/// BFS forward along Ddg edges from `source`. For every reached
/// node that is a sink (and not preceded by a sanitizer), record
/// a flow. Sanitizers terminate propagation through their node.
void bfs_from_source(
    const Graph& graph,
    NodeId source,
    const std::vector<ClassificationEntry>& roles,
    const TaintSpec& spec,
    std::vector<TaintFlow>& flows,
    AnalysisStats& stats) {

    std::unordered_set<std::uint32_t> visited;
    visited.insert(source.value);

    // Each frontier entry remembers the path so the emitted flow
    // carries the full chain. Memory cost is O(path length × outdegree)
    // which is fine for file-scale graphs.
    std::queue<std::vector<NodeId>> frontier;
    frontier.push({source});

    while (!frontier.empty()) {
        auto path = std::move(frontier.front());
        frontier.pop();
        NodeId current = path.back();
        ++stats.bfs_nodes_visited;

        // Reached a sink — emit a flow if the current node is the
        // sink (not the source itself, which classify_all may have
        // tagged as both).
        if (current != source && roles[current.value].role == Role::Sink) {
            const auto& sink_spec = spec.sinks[roles[current.value].spec_index];
            const auto& source_spec =
                spec.sources[roles[source.value].spec_index];
            flows.push_back(TaintFlow{
                .source_node    = source,
                .sink_node      = current,
                .path           = path,
                .source_spec_id = source_spec.id,
                .sink_spec_id   = sink_spec.id,
            });
            ++stats.flows_emitted;
            // Don't expand past the sink — once we've found this
            // sink the path through it is reported; deeper sinks
            // along the same chain are still reachable via other
            // frontier branches.
            continue;
        }

        // Sanitizer terminates propagation past this node.
        if (current != source && roles[current.value].role == Role::Sanitizer) {
            continue;
        }

        // Expand along Ddg edges. Iterating graph.edges() is O(E)
        // per pop, giving O(V·E) overall — fine for the per-file
        // graphs euxis scans.
        for (const auto& e : graph.edges()) {
            if (e.kind != EdgeKind::Ddg) continue;
            if (e.source != current) continue;
            if (visited.find(e.target.value) != visited.end()) continue;
            visited.insert(e.target.value);
            auto extended = path;
            extended.push_back(e.target);
            frontier.push(std::move(extended));
        }
    }
}

auto fnv1a(std::string_view a, std::string_view b,
           std::string_view c, std::string_view d,
           std::uint32_t e) -> std::uint64_t {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    auto absorb_str = [&](std::string_view s) {
        for (unsigned char ch : s) {
            h ^= ch;
            h *= 0x100000001b3ULL;
        }
    };
    absorb_str(a); absorb_str(b); absorb_str(c); absorb_str(d);
    for (int i = 3; i >= 0; --i) {
        h ^= (e >> (i * 8)) & 0xFFU;
        h *= 0x100000001b3ULL;
    }
    return h;
}

auto stable_fingerprint(std::string_view rule_id,
                        std::string_view file,
                        std::string_view source_raw_kind,
                        std::string_view sink_raw_kind,
                        std::uint32_t sink_line) -> std::string {
    std::uint64_t h = fnv1a(rule_id, file, source_raw_kind, sink_raw_kind, sink_line);
    static constexpr std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    std::string out;
    out.reserve(32);
    auto append = [&](std::uint64_t v) {
        for (int i = 15; i >= 0; --i) {
            out.push_back(hex[(v >> (i * 4)) & 0xF]);
        }
    };
    append(h);
    append(h ^ 0x5a5a5a5a5a5a5a5aULL);
    return out;
}

} // namespace

auto analyze(const euxis::cpg::Graph& graph,
             const euxis::parse::Ast& ast,
             const TaintSpec& spec) -> AnalysisResult {
    AnalysisResult result;
    if (graph.node_count() == 0) return result;

    auto roles = classify_all(graph, ast, spec, result.stats);

    for (const auto& node : graph.nodes()) {
        if (roles[node.id.value].role != Role::Source) continue;
        bfs_from_source(graph, node.id, roles, spec,
                        result.flows, result.stats);
    }
    return result;
}

auto flows_to_findings(const AnalysisResult& result,
                       const euxis::cpg::Graph& graph,
                       const std::string& file_path)
    -> std::vector<euxis::security::Finding> {
    std::vector<euxis::security::Finding> out;
    out.reserve(result.flows.size());

    for (const auto& flow : result.flows) {
        const Node* sink_node   = graph.get(flow.sink_node);
        const Node* source_node = graph.get(flow.source_node);
        if (sink_node == nullptr || source_node == nullptr) continue;

        euxis::security::Finding f;
        std::ostringstream rule_id;
        rule_id << "euxis-taint/" << flow.source_spec_id
                << "->" << flow.sink_spec_id;
        f.rule_id = rule_id.str();

        std::ostringstream msg;
        msg << "Tainted data from " << flow.source_spec_id
            << " reaches " << flow.sink_spec_id
            << " without sanitisation.";
        f.message = msg.str();

        f.severity   = euxis::security::Severity::High;
        f.confidence = euxis::security::Confidence::Probable;
        f.source     = euxis::security::RuleSource::LlmInferred;

        euxis::security::SourceLocation primary;
        primary.path         = file_path;
        primary.start_line   = static_cast<int>(sink_node->range.start_row + 1);
        primary.start_column = static_cast<int>(sink_node->range.start_column + 1);
        primary.end_line     = static_cast<int>(sink_node->range.end_row + 1);
        primary.end_column   = static_cast<int>(sink_node->range.end_column + 1);
        f.primary_location = primary;

        euxis::security::SourceLocation src_loc;
        src_loc.path         = file_path;
        src_loc.start_line   = static_cast<int>(source_node->range.start_row + 1);
        src_loc.start_column = static_cast<int>(source_node->range.start_column + 1);
        src_loc.end_line     = static_cast<int>(source_node->range.end_row + 1);
        src_loc.end_column   = static_cast<int>(source_node->range.end_column + 1);
        f.related_locations.push_back(src_loc);

        for (std::size_t i = 1; i + 1 < flow.path.size(); ++i) {
            const Node* hop = graph.get(flow.path[i]);
            if (hop == nullptr) continue;
            euxis::security::SourceLocation hop_loc;
            hop_loc.path         = file_path;
            hop_loc.start_line   = static_cast<int>(hop->range.start_row + 1);
            hop_loc.start_column = static_cast<int>(hop->range.start_column + 1);
            hop_loc.end_line     = static_cast<int>(hop->range.end_row + 1);
            hop_loc.end_column   = static_cast<int>(hop->range.end_column + 1);
            f.related_locations.push_back(hop_loc);
        }

        f.stable_fingerprint = stable_fingerprint(
            f.rule_id, file_path,
            source_node->raw_kind, sink_node->raw_kind,
            sink_node->range.start_byte);

        out.push_back(std::move(f));
    }
    return out;
}

} // namespace euxis::taint
