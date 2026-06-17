#include "euxis/reach/reachability.hpp"

#include <algorithm>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace euxis::reach {

namespace {

using euxis::cpg::Graph;
using euxis::cpg::NodeKind;
using euxis::cpg::NodeId;
using euxis::cpg::kNullNode;

bool is_top_level_function(const Graph& graph, NodeId fn) {
    NodeId current = graph.parent(fn);
    while (current.is_valid()) {
        const auto* n = graph.get(current);
        if (n != nullptr && n->kind == NodeKind::FunctionDef) return false;
        NodeId parent = graph.parent(current);
        if (!parent.is_valid() || parent == current) break;
        current = parent;
    }
    return true;
}

NodeId find_finding_function(
    const Graph& graph,
    const euxis::security::Finding& f,
    const std::unordered_map<std::string, std::vector<NodeId>>& fn_by_path) {
    auto it = fn_by_path.find(f.primary_location.path);
    if (it == fn_by_path.end()) return kNullNode;
    const auto target_row =
        static_cast<std::uint32_t>(f.primary_location.start_line > 0
            ? f.primary_location.start_line - 1
            : 0);

    // Find a FunctionDef whose range contains the finding row.
    // Functions are stored in the order they appeared in the
    // graph; we just scan linearly.
    for (NodeId fn : it->second) {
        const auto* n = graph.get(fn);
        if (n == nullptr) continue;
        if (target_row >= n->range.start_row &&
            target_row <= n->range.end_row) {
            return fn;
        }
    }
    return kNullNode;
}

} // namespace

auto compute_reachable(const Graph& graph,
                       const euxis::parse::Ast& /*ast*/,
                       const CallGraph& cg,
                       const ReachabilityConfig& config) -> ReachabilityResult {
    ReachabilityResult out;
    if (graph.node_count() == 0) return out;

    auto functions = graph.find_by_kind(NodeKind::FunctionDef);
    if (functions.empty()) return out;

    // Index by function id for fast lookup.
    std::unordered_map<std::string, std::vector<NodeId>> by_name;
    by_name.reserve(functions.size() * 2);

    // Build adjacency lists from the call edges so BFS doesn't
    // re-scan the whole edge vector per pop.
    std::unordered_map<std::uint32_t, std::vector<NodeId>> adj;
    adj.reserve(cg.edges.size());
    for (const auto& e : cg.edges) {
        adj[e.caller.value].push_back(e.callee);
    }

    // FunctionDef nodes carry an empty `name` (the builder only
    // populates names for Identifier nodes). Resolve the function
    // name by walking the FunctionDef's AstChild descendants and
    // returning the first non-empty Identifier name encountered.
    // For C the identifier sits inside `function_declarator`; for
    // Rust / Go / Python / JS / Java it is typically a direct
    // child. BFS-depth limit avoids walking into nested closures.
    auto function_name = [&](NodeId fn) -> std::string {
        std::queue<std::pair<NodeId, int>> q;
        q.push({fn, 0});
        constexpr int kMaxDepth = 3;
        while (!q.empty()) {
            auto [id, depth] = q.front();
            q.pop();
            if (depth > kMaxDepth) continue;
            for (NodeId c : graph.children(id)) {
                const cpg::Node* cn = graph.get(c);
                if (cn == nullptr) continue;
                if (cn->kind == NodeKind::Identifier && !cn->name.empty()) {
                    return cn->name;
                }
                if (cn->kind == NodeKind::FunctionDef) continue;
                q.push({c, depth + 1});
            }
        }
        return {};
    };

    // Compute entry-point set.
    std::queue<std::pair<NodeId, std::size_t>> frontier;
    for (NodeId fn : functions) {
        bool is_entry = false;
        if (config.top_level_functions_are_entries &&
            is_top_level_function(graph, fn)) {
            is_entry = true;
        }
        if (!is_entry && !config.entry_function_names.empty()) {
            auto resolved_name = function_name(fn);
            if (!resolved_name.empty() &&
                std::find(config.entry_function_names.begin(),
                          config.entry_function_names.end(),
                          resolved_name) !=
                config.entry_function_names.end()) {
                is_entry = true;
            }
        }
        if (is_entry) {
            ++out.entry_points;
            if (out.reachable_functions.insert(fn.value).second) {
                frontier.push({fn, 0});
            }
        }
    }

    // Also walk edges originating from "no caller" (file-scope
    // calls). They behave like extra entries.
    auto file_root_it = adj.find(0);
    if (file_root_it != adj.end()) {
        for (NodeId callee : file_root_it->second) {
            if (out.reachable_functions.insert(callee.value).second) {
                frontier.push({callee, 0});
            }
        }
    }

    while (!frontier.empty()) {
        auto [current, depth] = frontier.front();
        frontier.pop();
        if (config.max_depth > 0 && depth >= config.max_depth) continue;

        auto it = adj.find(current.value);
        if (it == adj.end()) continue;
        for (NodeId callee : it->second) {
            if (out.reachable_functions.insert(callee.value).second) {
                frontier.push({callee, depth + 1});
            }
        }
    }
    return out;
}

void annotate_findings(
    const Graph& graph,
    const ReachabilityResult& reach,
    const std::vector<euxis::security::Finding>::iterator begin,
    const std::vector<euxis::security::Finding>::iterator end) {

    // Pre-index FunctionDefs by their primary location path so the
    // per-finding lookup is O(1) on average. The graph + ast pair
    // is per-file in the v0.0.3 surface so every function will
    // typically share the same path.
    std::unordered_map<std::string, std::vector<NodeId>> by_path;
    for (NodeId fn : graph.find_by_kind(NodeKind::FunctionDef)) {
        const auto* n = graph.get(fn);
        if (n == nullptr) continue;
        // FunctionDef nodes don't carry an explicit path — their
        // location is the file the AST was parsed from. The
        // finding's primary_location.path is the same. Group
        // every FunctionDef under that path so the lookup hits.
        by_path[(begin != end) ? begin->primary_location.path
                                : std::string{}].push_back(fn);
    }
    // If the finding range carries a different path, fall back to
    // grouping under it too — the indexer above is conservative.
    for (auto it = begin; it != end; ++it) {
        if (by_path.find(it->primary_location.path) == by_path.end()) {
            for (NodeId fn : graph.find_by_kind(NodeKind::FunctionDef)) {
                by_path[it->primary_location.path].push_back(fn);
            }
            break;
        }
    }

    for (auto it = begin; it != end; ++it) {
        NodeId fn = find_finding_function(graph, *it, by_path);
        if (!fn.is_valid()) {
            it->compliance_taxa.emplace_back("euxis:reachable:unknown");
            continue;
        }
        if (reach.reachable_functions.find(fn.value) !=
            reach.reachable_functions.end()) {
            it->compliance_taxa.emplace_back("euxis:reachable:true");
        } else {
            it->compliance_taxa.emplace_back("euxis:reachable:false");
        }
    }
}

} // namespace euxis::reach
