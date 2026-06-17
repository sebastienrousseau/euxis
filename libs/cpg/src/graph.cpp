#include "euxis/cpg/graph.hpp"

#include <algorithm>
#include <cassert>

namespace euxis::cpg {

Graph::Graph() = default;

auto Graph::node_count() const noexcept -> std::size_t {
    return nodes_.size();
}

auto Graph::edge_count() const noexcept -> std::size_t {
    return edges_.size();
}

auto Graph::nodes() const noexcept -> std::span<const Node> {
    return std::span<const Node>{nodes_.data(), nodes_.size()};
}

auto Graph::edges() const noexcept -> std::span<const Edge> {
    return std::span<const Edge>{edges_.data(), edges_.size()};
}

auto Graph::get(NodeId id) const noexcept -> const Node* {
    if (!id.is_valid()) return nullptr;
    std::size_t idx = id.value - 1U;  // ids are 1-based; index 0 = id 1
    if (idx >= nodes_.size()) return nullptr;
    return &nodes_[idx];
}

auto Graph::root() const noexcept -> NodeId { return root_; }

auto Graph::add_node(Node n) -> NodeId {
    NodeId id{static_cast<std::uint32_t>(nodes_.size() + 1)};
    n.id = id;
    nodes_.push_back(std::move(n));
    if (!root_.is_valid()) {
        root_ = id;
    }
    return id;
}

void Graph::add_edge(Edge e) {
    assert(e.source.is_valid() && "Graph::add_edge — source must be valid");
    assert(e.target.is_valid() && "Graph::add_edge — target must be valid");
    edges_.push_back(e);
}

auto Graph::find_by_kind(NodeKind kind) const -> std::vector<NodeId> {
    std::vector<NodeId> out;
    out.reserve(8);
    for (const auto& n : nodes_) {
        if (n.kind == kind) out.push_back(n.id);
    }
    return out;
}

auto Graph::find_if(const std::function<bool(const Node&)>& pred) const
    -> std::vector<NodeId> {
    std::vector<NodeId> out;
    if (!pred) return out;
    out.reserve(8);
    for (const auto& n : nodes_) {
        if (pred(n)) out.push_back(n.id);
    }
    return out;
}

auto Graph::children(NodeId parent_id) const -> std::vector<NodeId> {
    std::vector<NodeId> out;
    if (!parent_id.is_valid()) return out;
    for (const auto& e : edges_) {
        if (e.kind == EdgeKind::AstChild && e.source == parent_id) {
            out.push_back(e.target);
        }
    }
    return out;
}

auto Graph::parent(NodeId child) const noexcept -> NodeId {
    if (!child.is_valid()) return kNullNode;
    for (const auto& e : edges_) {
        if (e.kind == EdgeKind::AstChild && e.target == child) {
            return e.source;
        }
    }
    return kNullNode;
}

auto Graph::descendant_count(NodeId root_id) const noexcept -> std::size_t {
    if (!root_id.is_valid()) return 0;
    std::size_t total = 1;  // include `root_id` itself
    // Frontier DFS keeps recursion bounded. The dense edge vector
    // means this is O(N*E) worst case, but real ASTs are shallow.
    std::vector<NodeId> frontier{root_id};
    while (!frontier.empty()) {
        NodeId current = frontier.back();
        frontier.pop_back();
        for (const auto& e : edges_) {
            if (e.kind == EdgeKind::AstChild && e.source == current) {
                total += 1;
                frontier.push_back(e.target);
            }
        }
    }
    return total;
}

} // namespace euxis::cpg
