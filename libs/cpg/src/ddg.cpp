#include "euxis/cpg/ddg.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace euxis::cpg {

namespace {

struct DdgWalker {
    Graph& graph;
    const DdgBuildOptions& opts;
    DdgStats stats{};
    std::unordered_map<std::string, NodeId> def_map;

    /// Look up `name` in the def map. If a definition is recorded,
    /// emit a Ddg edge from that definition to `use_id`.
    void emit_use(NodeId use_id, std::string_view name) {
        if (name.empty()) return;
        auto it = def_map.find(std::string{name});
        if (it == def_map.end()) return;
        if (it->second == use_id) return;  // would be a self-edge
        graph.add_edge(Edge{
            .source = it->second,
            .target = use_id,
            .kind   = EdgeKind::Ddg,
        });
        ++stats.ddg_edges;
    }

    /// Record `def_id` as the most recent definition of `name`.
    void record_def(std::string_view name, NodeId def_id) {
        if (name.empty() || !def_id.is_valid()) return;
        if (def_map.size() >= opts.max_names_per_function &&
            def_map.find(std::string{name}) == def_map.end()) {
            ++stats.names_capped;
            return;
        }
        def_map[std::string{name}] = def_id;
        ++stats.definitions_recorded;
    }

    /// AstChild Identifier names defined by a Declaration or
    /// Assignment node. tree-sitter grammars vary on whether the
    /// identifier is a direct child (Rust `let_declaration` ->
    /// `identifier`) or nested under a wrapper (C/Cpp `declaration`
    /// -> `init_declarator` -> `identifier`; JS `lexical_declaration`
    /// -> `variable_declarator` -> `identifier`). We collect direct
    /// Identifier children first, then if none were found we descend
    /// one level through every non-Identifier child to handle the
    /// wrapped grammars. The single-level descent is conservative —
    /// we never walk deep enough to mis-attribute nested
    /// sub-expression identifiers as the LHS being defined.
    std::vector<std::pair<NodeId, std::string>> identifier_children(
            NodeId node_id) const {
        std::vector<std::pair<NodeId, std::string>> out;
        auto try_collect = [&](NodeId c) -> bool {
            const Node* cn = graph.get(c);
            if (cn != nullptr && cn->kind == NodeKind::Identifier &&
                !cn->name.empty()) {
                out.emplace_back(c, cn->name);
                return true;
            }
            return false;
        };
        for (NodeId c : graph.children(node_id)) {
            if (try_collect(c)) continue;
            for (NodeId gc : graph.children(c)) {
                if (try_collect(gc)) break;
            }
        }
        return out;
    }

    /// Recursive walk of an arbitrary subtree. Treats every
    /// Identifier as a use; dispatches Declaration and Assignment
    /// to their dedicated handlers so the LHS/RHS distinction is
    /// preserved. Parameter nodes are skipped because they were
    /// already consumed by `register_parameters()`.
    void walk(NodeId id) {
        const Node* n = graph.get(id);
        if (n == nullptr) return;

        switch (n->kind) {
            case NodeKind::Parameter:
                return;
            case NodeKind::Identifier:
                emit_use(id, n->name);
                return;
            case NodeKind::Declaration:
                walk_declaration(id);
                return;
            case NodeKind::Assignment:
                walk_assignment(id);
                return;
            default:
                for (NodeId c : graph.children(id)) walk(c);
                return;
        }
    }

    /// Declarations: walk every child (so RHS identifiers register
    /// as uses against any prior bindings), then record each
    /// direct Identifier child as a new definition pointing at
    /// the Declaration node.
    void walk_declaration(NodeId id) {
        if (!opts.track_declarations) {
            for (NodeId c : graph.children(id)) walk(c);
            return;
        }

        auto idents = identifier_children(id);
        for (NodeId c : graph.children(id)) {
            // Skip the direct Identifier-name children — those
            // are the things being defined, not used.
            const Node* cn = graph.get(c);
            if (cn != nullptr && cn->kind == NodeKind::Identifier &&
                !cn->name.empty()) {
                continue;
            }
            walk(c);
        }
        for (const auto& [child_id, name] : idents) {
            record_def(name, id);
        }
    }

    /// Assignments: process RHS first so the LHS identifier isn't
    /// counted as a use of the prior def of itself, then record
    /// the LHS as the new definition.
    void walk_assignment(NodeId id) {
        auto children = graph.children(id);
        if (children.empty()) return;

        const Node* lhs = graph.get(children.front());
        std::string lhs_name;
        if (lhs != nullptr && lhs->kind == NodeKind::Identifier) {
            lhs_name = lhs->name;
        }

        // Walk RHS subtrees (everything after the LHS).
        for (std::size_t i = 1; i < children.size(); ++i) {
            walk(children[i]);
        }

        // If the LHS is more complex than a bare Identifier (a
        // member access, indexed write, etc.), walk it too — the
        // base object name still counts as a use.
        if (lhs != nullptr && lhs->kind != NodeKind::Identifier) {
            walk(children.front());
        }

        // Record the LHS name as a new definition.
        if (opts.track_assignments && !lhs_name.empty()) {
            record_def(lhs_name, id);
        }
    }

    /// Register every Parameter node found anywhere in the
    /// subtree rooted at `root`, except inside Block bodies
    /// (which contain *uses* of the parameter names, not
    /// further definitions). Handles tree-sitter grammars
    /// where Parameter nodes are nested under a wrapper
    /// (C/C++ `function_declarator` -> `parameter_list`
    /// -> `parameter_declaration`; JS `formal_parameters`).
    void register_parameters(NodeId root) {
        for (NodeId c : graph.children(root)) {
            const Node* cn = graph.get(c);
            if (cn == nullptr) continue;
            if (cn->kind == NodeKind::Parameter) {
                for (const auto& [pid, pname] : identifier_children(c)) {
                    record_def(pname, c);
                }
            } else if (cn->kind != NodeKind::Block &&
                       cn->kind != NodeKind::FunctionDef) {
                register_parameters(c);
            }
        }
    }

    void process_function(NodeId fn) {
        ++stats.functions_processed;
        // Each function gets a fresh local def map. Function-scope
        // is the unit of analysis the Week 10 approximation
        // commits to; parameter names land as initial bindings.
        def_map.clear();
        register_parameters(fn);

        for (NodeId child : graph.children(fn)) {
            const Node* cn = graph.get(child);
            if (cn == nullptr) continue;
            // Parameters were registered above; skip them so their
            // identifier children don't get walked as uses.
            if (cn->kind == NodeKind::Parameter) continue;
            walk(child);
        }
    }
};

} // namespace

auto build_ddg(Graph& graph, const DdgBuildOptions& opts) -> DdgStats {
    DdgWalker w{graph, opts, {}, {}};
    for (NodeId fn : graph.find_by_kind(NodeKind::FunctionDef)) {
        w.process_function(fn);
    }
    return w.stats;
}

} // namespace euxis::cpg
