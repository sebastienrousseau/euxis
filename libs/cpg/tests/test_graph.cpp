#include <gtest/gtest.h>

#include "euxis/cpg/graph.hpp"

namespace euxis::cpg {
namespace {

Node make_node(NodeKind kind, std::string raw = "raw") {
    Node n;
    n.kind     = kind;
    n.raw_kind = std::move(raw);
    return n;
}

TEST(Graph, EmptyByDefault) {
    Graph g;
    EXPECT_EQ(g.node_count(), 0U);
    EXPECT_EQ(g.edge_count(), 0U);
    EXPECT_FALSE(g.root().is_valid());
}

TEST(Graph, AddNodeReturnsDenseIds) {
    Graph g;
    auto a = g.add_node(make_node(NodeKind::TranslationUnit));
    auto b = g.add_node(make_node(NodeKind::FunctionDef));
    auto c = g.add_node(make_node(NodeKind::Block));
    EXPECT_EQ(a.value, 1U);
    EXPECT_EQ(b.value, 2U);
    EXPECT_EQ(c.value, 3U);
    EXPECT_EQ(g.node_count(), 3U);
}

TEST(Graph, RootIsTheFirstAddedNode) {
    Graph g;
    auto a = g.add_node(make_node(NodeKind::TranslationUnit));
    g.add_node(make_node(NodeKind::FunctionDef));
    EXPECT_EQ(g.root(), a);
}

TEST(Graph, GetResolvesValidIds) {
    Graph g;
    auto a = g.add_node(make_node(NodeKind::FunctionDef, "function_definition"));
    const Node* n = g.get(a);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->kind, NodeKind::FunctionDef);
    EXPECT_EQ(n->raw_kind, "function_definition");
    EXPECT_EQ(n->id, a);
}

TEST(Graph, GetReturnsNullptrForInvalidId) {
    Graph g;
    g.add_node(make_node(NodeKind::FunctionDef));
    EXPECT_EQ(g.get(kNullNode),    nullptr);
    EXPECT_EQ(g.get(NodeId{9999}), nullptr);
}

TEST(Graph, AstChildEdgesTraversed) {
    Graph g;
    auto root = g.add_node(make_node(NodeKind::TranslationUnit));
    auto fn   = g.add_node(make_node(NodeKind::FunctionDef));
    auto blk  = g.add_node(make_node(NodeKind::Block));
    g.add_edge({.source = root, .target = fn,  .kind = EdgeKind::AstChild});
    g.add_edge({.source = fn,   .target = blk, .kind = EdgeKind::AstChild});

    auto root_children = g.children(root);
    ASSERT_EQ(root_children.size(), 1U);
    EXPECT_EQ(root_children[0], fn);

    auto fn_children = g.children(fn);
    ASSERT_EQ(fn_children.size(), 1U);
    EXPECT_EQ(fn_children[0], blk);

    EXPECT_EQ(g.parent(blk), fn);
    EXPECT_EQ(g.parent(fn),  root);
    EXPECT_EQ(g.parent(root), kNullNode);
}

TEST(Graph, FindByKindReturnsAllMatches) {
    Graph g;
    g.add_node(make_node(NodeKind::TranslationUnit));
    auto a = g.add_node(make_node(NodeKind::Identifier));
    g.add_node(make_node(NodeKind::Block));
    auto b = g.add_node(make_node(NodeKind::Identifier));
    auto c = g.add_node(make_node(NodeKind::Identifier));
    auto idents = g.find_by_kind(NodeKind::Identifier);
    ASSERT_EQ(idents.size(), 3U);
    EXPECT_EQ(idents[0], a);
    EXPECT_EQ(idents[1], b);
    EXPECT_EQ(idents[2], c);
}

TEST(Graph, FindIfPredicateMatches) {
    Graph g;
    g.add_node(make_node(NodeKind::FunctionDef, "function_definition"));
    g.add_node(make_node(NodeKind::FunctionDef, "method_declaration"));
    g.add_node(make_node(NodeKind::Block));
    auto only_methods = g.find_if([](const Node& n) {
        return n.raw_kind == "method_declaration";
    });
    ASSERT_EQ(only_methods.size(), 1U);
}

TEST(Graph, FindIfHandlesEmptyPredicateGracefully) {
    Graph g;
    g.add_node(make_node(NodeKind::FunctionDef));
    auto out = g.find_if(std::function<bool(const Node&)>{});
    EXPECT_TRUE(out.empty());
}

TEST(Graph, DescendantCountWalksTree) {
    Graph g;
    auto root = g.add_node(make_node(NodeKind::TranslationUnit));
    auto fn   = g.add_node(make_node(NodeKind::FunctionDef));
    auto blk  = g.add_node(make_node(NodeKind::Block));
    auto a    = g.add_node(make_node(NodeKind::Identifier));
    auto b    = g.add_node(make_node(NodeKind::Identifier));
    g.add_edge({.source = root, .target = fn,  .kind = EdgeKind::AstChild});
    g.add_edge({.source = fn,   .target = blk, .kind = EdgeKind::AstChild});
    g.add_edge({.source = blk,  .target = a,   .kind = EdgeKind::AstChild});
    g.add_edge({.source = blk,  .target = b,   .kind = EdgeKind::AstChild});
    EXPECT_EQ(g.descendant_count(root), 5U);
    EXPECT_EQ(g.descendant_count(blk),  3U);
    EXPECT_EQ(g.descendant_count(a),    1U);
}

TEST(Graph, EdgeCountReflectsInsertions) {
    Graph g;
    auto root = g.add_node(make_node(NodeKind::TranslationUnit));
    auto fn   = g.add_node(make_node(NodeKind::FunctionDef));
    g.add_edge({.source = root, .target = fn, .kind = EdgeKind::AstChild});
    EXPECT_EQ(g.edge_count(), 1U);
}

} // namespace
} // namespace euxis::cpg
