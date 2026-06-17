#include "euxis/cpg/builder.hpp"

#include <string>
#include <utility>
#include <vector>

namespace euxis::cpg {

namespace {

/// Heuristic name extraction: for FunctionDef / Call / Identifier
/// nodes the AST node text is often the identifier itself or a
/// surrounding form like `identifier(args)`. We don't run a real
/// extraction (that needs grammar-specific queries) yet — for
/// week 8a the name is left blank for non-Identifier kinds and the
/// raw_kind plus source range is enough for any consumer that
/// needs more. The identifier-name extraction lands with the CPG
/// query layer in P1.4.
auto extract_name(NodeKind kind, std::string_view text) -> std::string {
    if (kind != NodeKind::Identifier) return {};
    // Identifier nodes ARE their name in every grammar libs/parse
    // ships today, so we copy the source bytes verbatim.
    return std::string{text};
}

struct Frame {
    NodeId parent;
};

} // namespace

auto build(const euxis::parse::Ast& ast)
    -> std::expected<Graph, BuildError> {
    if (ast.source().empty() && ast.root_named_child_count() == 0) {
        // Empty Ast → empty Graph. Not an error; downstream code
        // already handles zero-finding scans.
    }

    Graph g;

    // The visitor walks named nodes in pre-order with a parent
    // stack. tree-sitter doesn't give us a parent token on the way
    // down, so we maintain our own stack and consult source ranges
    // (range containment) to pop frames when we leave a subtree.
    //
    // libs/parse's visit_named_nodes already iterates in pre-order
    // and supports short-circuit descent; we lean on that ordering
    // to determine parentage by comparing the visiting node's
    // start_byte against the current frame's range.

    std::vector<std::pair<NodeId, euxis::parse::SourceRange>> stack;
    stack.reserve(64);

    const auto lang = ast.language();

    auto get_text = [&](euxis::parse::SourceRange r) -> std::string_view {
        return ast.node_text(r);
    };

    ast.visit_named_nodes(
        [&](std::string_view raw_kind, euxis::parse::SourceRange range) {
            // Pop frames whose range ends before this node starts.
            // This keeps the stack invariant: the top frame is the
            // smallest ancestor that contains `range`.
            while (!stack.empty() &&
                   stack.back().second.end_byte <= range.start_byte) {
                stack.pop_back();
            }

            Node n;
            n.kind     = classify(lang, raw_kind);
            n.raw_kind = std::string{raw_kind};
            n.range    = range;
            n.language = lang;
            n.name     = extract_name(n.kind, get_text(range));

            NodeId id = g.add_node(std::move(n));

            if (!stack.empty()) {
                g.add_edge(Edge{
                    .source = stack.back().first,
                    .target = id,
                    .kind   = EdgeKind::AstChild,
                });
            }

            stack.emplace_back(id, range);
            return true;  // continue descent into this node's children
        });

    return g;
}

} // namespace euxis::cpg
