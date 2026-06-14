#include "euxis/reach/callgraph.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace euxis::reach {

namespace {

/// Pull a likely function-name token out of a node's source text.
/// Tokens are `[A-Za-z_][A-Za-z0-9_]*`; we collect every token and
/// let the caller decide which one to use as the function name.
auto identifier_tokens(std::string_view text) -> std::vector<std::string> {
    std::vector<std::string> out;
    std::string current;
    auto flush = [&] {
        if (current.size() >= 2) out.push_back(current);
        current.clear();
    };
    for (char c : text) {
        if ((std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_') {
            current.push_back(c);
        } else {
            flush();
        }
    }
    flush();
    return out;
}

/// Best-effort function-name extraction: walk the FunctionDef's
/// AstChild descendants looking for the leftmost Identifier that
/// is NOT a parameter. Falls back to the first identifier token in
/// the function's source text when no Identifier node is found.
std::string extract_function_name(const euxis::cpg::Graph& graph,
                                  const euxis::parse::Ast& ast,
                                  euxis::cpg::NodeId fn_id) {
    using euxis::cpg::NodeKind;
    using euxis::cpg::NodeId;

    // First pass: leftmost Identifier among direct/grand children.
    std::vector<NodeId> frontier{fn_id};
    while (!frontier.empty()) {
        NodeId current = frontier.front();
        frontier.erase(frontier.begin());
        for (NodeId child : graph.children(current)) {
            const auto* node = graph.get(child);
            if (node == nullptr) continue;
            if (node->kind == NodeKind::Parameter ||
                node->kind == NodeKind::Block) {
                // Skip into parameters (no name there) and bodies
                // (the function name is BEFORE the body).
                continue;
            }
            if (node->kind == NodeKind::Identifier && !node->name.empty()) {
                return node->name;
            }
            // Descend into other wrapper nodes (declarator etc.).
            frontier.push_back(child);
        }
    }

    // Fallback: identifier tokens from the function's text.
    auto tokens = identifier_tokens(ast.node_text(graph.get(fn_id)->range));
    if (!tokens.empty()) return tokens.front();
    return {};
}

/// Pull the called-function identifier out of a Call node's text.
/// We deliberately take the first identifier-shaped token, which
/// matches direct calls (`foo(args)`) and the receiver of method
/// calls (`object.method(args)` -> "object"). The latter is not
/// strictly the callee but is good enough for the over-approximate
/// reachability story; the receiver name often coincides with the
/// invoked method when the receiver IS a free function reference.
std::string extract_callee_name(std::string_view text) {
    auto tokens = identifier_tokens(text);
    return tokens.empty() ? std::string{} : tokens.front();
}

} // namespace

auto enclosing_function(const euxis::cpg::Graph& graph,
                        euxis::cpg::NodeId node) noexcept
    -> euxis::cpg::NodeId {
    using euxis::cpg::NodeKind;
    using euxis::cpg::NodeId;
    using euxis::cpg::kNullNode;

    NodeId current = node;
    while (current.is_valid()) {
        const auto* n = graph.get(current);
        if (n != nullptr && n->kind == NodeKind::FunctionDef) {
            return current;
        }
        NodeId parent = graph.parent(current);
        if (!parent.is_valid() || parent == current) break;
        current = parent;
    }
    return kNullNode;
}

auto build_call_graph(const euxis::cpg::Graph& graph,
                      const euxis::parse::Ast& ast)
    -> CallGraphResult {
    using euxis::cpg::NodeKind;
    using euxis::cpg::NodeId;

    CallGraphResult result;
    auto& stats = result.stats;

    auto functions = graph.find_by_kind(NodeKind::FunctionDef);
    stats.function_count = functions.size();

    // Index functions by their extracted name so call sites can be
    // resolved in O(1). Multiple functions may share a name (C++
    // overloads, two methods of different classes) — we keep every
    // match and emit an edge for each. The reachability BFS then
    // unions them.
    std::unordered_map<std::string, std::vector<NodeId>> by_name;
    by_name.reserve(functions.size() * 2);
    for (NodeId fn : functions) {
        auto name = extract_function_name(graph, ast, fn);
        if (name.empty()) continue;
        by_name[name].push_back(fn);
    }

    auto calls = graph.find_by_kind(NodeKind::Call);
    stats.call_sites = calls.size();

    for (NodeId call : calls) {
        const auto* call_node = graph.get(call);
        if (call_node == nullptr) continue;
        auto callee_name = extract_callee_name(ast.node_text(call_node->range));
        if (callee_name.empty()) {
            ++stats.unresolved_calls;
            continue;
        }
        auto it = by_name.find(callee_name);
        if (it == by_name.end()) {
            ++stats.unresolved_calls;
            continue;
        }
        NodeId caller = enclosing_function(graph, call);
        if (!caller.is_valid()) {
            // Call sits at the file scope (e.g. Python module-level
            // code). We model that as "the file" being the caller;
            // the BFS treats unresolved-caller edges as entries.
            for (NodeId callee : it->second) {
                result.graph.edges.push_back({.caller = NodeId{0},
                                              .callee = callee});
            }
        } else {
            for (NodeId callee : it->second) {
                result.graph.edges.push_back({.caller = caller,
                                              .callee = callee});
            }
        }
        ++stats.resolved_calls;
    }

    return result;
}

} // namespace euxis::reach
