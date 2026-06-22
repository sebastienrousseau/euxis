/// @file
/// @brief Vector-backed CPG storage with a small query surface.
///
/// `Graph` owns the nodes and edges produced by a Builder. Storage
/// is vector-of-Node + vector-of-Edge; ids are dense and the graph
/// supports cheap iteration by NodeKind / EdgeKind for the analyses
/// in libs/scan and libs/security that consume CPGs.
///
/// The query API is intentionally narrow at this stage: predicate-
/// driven filters, parent/child traversal, find-by-kind. Anything
/// resembling Cypher / Joern's CPGQL goes through a higher-level
/// query layer that lands with the OpenGrep rule-pack ingester
/// (P1.4) — we don't want to commit to a query language until at
/// least one consumer has shaped its requirements.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "euxis/cpg/types.hpp"

namespace euxis::cpg {

class Graph {
public:
    Graph();
    ~Graph() = default;

    Graph(const Graph&)            = delete;
    Graph& operator=(const Graph&) = delete;
    Graph(Graph&&) noexcept        = default;
    Graph& operator=(Graph&&) noexcept = default;

    // ---- size + access ----

    [[nodiscard]] auto node_count() const noexcept -> std::size_t;
    [[nodiscard]] auto edge_count() const noexcept -> std::size_t;

    /// View into all nodes (read-only). Order is insertion order;
    /// id N is at index N-1 (id 0 is reserved as kNullNode).
    [[nodiscard]] auto nodes() const noexcept -> std::span<const Node>;

    /// View into all edges (read-only). Edge order is insertion
    /// order, which the Builder uses to preserve AST traversal
    /// ordering for downstream analyses.
    [[nodiscard]] auto edges() const noexcept -> std::span<const Edge>;

    /// Resolve a NodeId. Returns nullptr for kNullNode / out-of-range.
    [[nodiscard]] auto get(NodeId id) const noexcept -> const Node*;

    /// The root node of the graph (the TranslationUnit), or
    /// kNullNode if no nodes have been added.
    [[nodiscard]] auto root() const noexcept -> NodeId;

    // ---- mutation ----

    /// Append a node and return its id. The id is `node_count()`
    /// before the call (i.e. the node is placed at the end of the
    /// vector and the returned id is dense).
    auto add_node(Node n) -> NodeId;

    /// Append an edge. Caller is responsible for ensuring source +
    /// target are valid; debug-mode assertions catch the obvious
    /// errors.
    void add_edge(Edge e);

    // ---- queries ----

    /// Return every node with `kind`.
    [[nodiscard]] auto find_by_kind(NodeKind kind) const
        -> std::vector<NodeId>;

    /// Return every node whose predicate returns true.
    [[nodiscard]] auto find_if(
        const std::function<bool(const Node&)>& pred) const
        -> std::vector<NodeId>;

    /// AstChild children of `parent`. Returns ids in insertion
    /// order, so two scans of the same source produce the same
    /// child list — important for reproducible evidence packs.
    [[nodiscard]] auto children(NodeId parent) const
        -> std::vector<NodeId>;

    /// Parent of `child` per AstChild edges. The CPG can have
    /// multiple parents per node only across edge kinds (Call,
    /// Ddg), so per-edge-kind callers can safely assume one
    /// AstChild parent. Returns kNullNode if `child` is the root
    /// or has no AstChild parent.
    [[nodiscard]] auto parent(NodeId child) const noexcept -> NodeId;

    /// Sum-of-AstChild-descendants count of `root` (inclusive).
    [[nodiscard]] auto descendant_count(NodeId root) const
        -> std::size_t;

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    NodeId root_{kNullNode};
};

} // namespace euxis::cpg
