#include "euxis/cpg/cfg.hpp"

#include <cstddef>
#include <vector>

namespace euxis::cpg {

namespace {

struct CfgBuilder {
    Graph& graph;
    const CfgBuildOptions& opts;
    CfgStats stats{};

    void add(NodeId from, NodeId to) {
        if (!from.is_valid() || !to.is_valid() || from == to) return;
        graph.add_edge(Edge{
            .source = from,
            .target = to,
            .kind   = EdgeKind::Cfg,
        });
    }

    /// Connect consecutive direct AstChild children of `block` with
    /// sequential Cfg edges. Recurses into any nested Block / If /
    /// Loop encountered among the children.
    void process_block(NodeId block) {
        auto children = graph.children(block);
        for (std::size_t i = 0; i + 1 < children.size(); ++i) {
            add(children[i], children[i + 1]);
            ++stats.sequential_edges;
        }
        for (NodeId child : children) {
            descend(child);
        }
    }

    /// Walk into a node and dispatch on its kind. Only kinds that
    /// can contain executable structure (Block, If, Loop, FunctionDef)
    /// need handling; the rest are leaves from CFG's perspective.
    void descend(NodeId id) {
        const Node* n = graph.get(id);
        if (n == nullptr) return;
        switch (n->kind) {
            case NodeKind::Block:
                process_block(id);
                break;
            case NodeKind::If:
                process_if(id);
                break;
            case NodeKind::Loop:
                process_loop(id);
                break;
            default:
                // Many statement-shaped nodes (Return, Assignment,
                // Call, Declaration, …) carry a nested Block in
                // their AST when, e.g., the right-hand side is a
                // braced initialiser or a lambda. Descend so the
                // sequential edges inside that nested block still
                // get emitted.
                for (NodeId grand : graph.children(id)) {
                    const Node* gn = graph.get(grand);
                    if (gn != nullptr &&
                        (gn->kind == NodeKind::Block ||
                         gn->kind == NodeKind::If ||
                         gn->kind == NodeKind::Loop)) {
                        descend(grand);
                    }
                }
                break;
        }
    }

    /// Emit branch-entry edges from `if_node` to the first
    /// AstChild Block of each branch. tree-sitter grammars differ
    /// on whether the else-branch is a direct Block or wrapped in
    /// an else_clause node; we look one level deep for an
    /// AstChild Block in each case.
    void process_if(NodeId if_node) {
        if (!opts.emit_if_branch_edges) {
            // Even when branch edges are off, recurse so nested
            // blocks still get sequential edges.
            for (NodeId child : graph.children(if_node)) {
                descend(child);
            }
            return;
        }

        bool saw_first_block = false;
        for (NodeId child : graph.children(if_node)) {
            const Node* cn = graph.get(child);
            if (cn == nullptr) continue;

            // Direct Block child = then / else branch.
            if (cn->kind == NodeKind::Block) {
                add(if_node, first_child_of(child));
                ++stats.branch_entry_edges;
                saw_first_block = true;
                descend(child);
                continue;
            }

            // Search one level deeper for else_clause-style
            // wrappers that carry the Block as a grandchild.
            for (NodeId grand : graph.children(child)) {
                const Node* gn = graph.get(grand);
                if (gn != nullptr && gn->kind == NodeKind::Block) {
                    add(if_node, first_child_of(grand));
                    ++stats.branch_entry_edges;
                    saw_first_block = true;
                    descend(grand);
                }
            }

            // Always descend into non-block children so any nested
            // structure inside the condition gets its CFG built.
            if (cn->kind != NodeKind::Block) {
                descend(child);
            }
        }
        (void)saw_first_block;  // currently informational; will gate
                                 // a future "If without then-block" warning
    }

    /// Emit a back-edge from the last statement of the loop body
    /// to the Loop node, plus the entry edge into the body.
    void process_loop(NodeId loop_node) {
        if (!opts.emit_loop_back_edges) {
            for (NodeId child : graph.children(loop_node)) {
                descend(child);
            }
            return;
        }

        NodeId body{};
        for (NodeId child : graph.children(loop_node)) {
            const Node* cn = graph.get(child);
            if (cn != nullptr && cn->kind == NodeKind::Block) {
                body = child;
                break;
            }
        }

        if (body.is_valid()) {
            auto body_children = graph.children(body);
            if (!body_children.empty()) {
                add(loop_node, body_children.front());
                ++stats.branch_entry_edges;
                add(body_children.back(), loop_node);
                ++stats.loop_back_edges;
            }
        }

        // Sequence edges inside the body and any deeper nesting.
        for (NodeId child : graph.children(loop_node)) {
            descend(child);
        }
    }

    /// First AstChild of `id`, or `id` itself if it has no
    /// children. Used so branch-entry edges target the first real
    /// statement rather than the wrapping Block — taint engines
    /// that walk Cfg edges want to land on something concrete.
    NodeId first_child_of(NodeId id) const {
        auto children = graph.children(id);
        return children.empty() ? id : children.front();
    }

    void process_function(NodeId fn) {
        ++stats.functions_processed;
        for (NodeId child : graph.children(fn)) {
            descend(child);
        }
    }
};

} // namespace

auto build_cfg(Graph& graph, const CfgBuildOptions& opts) -> CfgStats {
    CfgBuilder b{graph, opts, {}};
    for (NodeId fn : graph.find_by_kind(NodeKind::FunctionDef)) {
        b.process_function(fn);
    }
    return b.stats;
}

} // namespace euxis::cpg
