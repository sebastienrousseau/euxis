#include <gtest/gtest.h>

#include "euxis/cpg/builder.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/reach/callgraph.hpp"

namespace euxis::reach {
namespace {

using euxis::parse::Language;
using euxis::parse::Parser;

struct Pipeline {
    euxis::parse::Ast ast;
    euxis::cpg::Graph graph;
};

Pipeline build_pipeline(Language lang, const std::string& src) {
    Parser p(lang);
    auto ast = p.parse(src);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto g = euxis::cpg::build(*ast);
    EXPECT_TRUE(g.has_value());
    return Pipeline{std::move(*ast), std::move(*g)};
}

TEST(CallGraph, EmptyAstProducesEmptyGraph) {
    Parser p(Language::C);
    auto ast = p.parse("");
    ASSERT_TRUE(ast.has_value());
    auto g = euxis::cpg::build(*ast);
    ASSERT_TRUE(g.has_value());
    auto result = build_call_graph(*g, *ast);
    EXPECT_EQ(result.graph.edges.size(), 0U);
}

TEST(CallGraph, DirectCallEdgeIsResolved) {
    auto pipe = build_pipeline(Language::C, R"(
int add(int a, int b) { return a + b; }
int main(void) { return add(1, 2); }
)");
    auto result = build_call_graph(pipe.graph, pipe.ast);
    EXPECT_GT(result.stats.function_count, 0U);
    EXPECT_GT(result.stats.call_sites, 0U);
    // At least one (main -> add) edge should appear.
    EXPECT_GT(result.graph.edges.size(), 0U);
    EXPECT_GT(result.stats.resolved_calls, 0U);
}

TEST(CallGraph, UnresolvedCallsAreCounted) {
    auto pipe = build_pipeline(Language::C, R"(
int main(void) { return unknown_external_fn(42); }
)");
    auto result = build_call_graph(pipe.graph, pipe.ast);
    EXPECT_GT(result.stats.unresolved_calls, 0U);
}

TEST(CallGraph, EnclosingFunctionWalksUp) {
    auto pipe = build_pipeline(Language::C, R"(
int outer(void) {
    int x = 1;
    return x;
}
)");
    auto idents = pipe.graph.find_by_kind(euxis::cpg::NodeKind::Identifier);
    ASSERT_FALSE(idents.empty());
    auto fns = pipe.graph.find_by_kind(euxis::cpg::NodeKind::FunctionDef);
    ASSERT_FALSE(fns.empty());

    // Every Identifier inside the function should resolve to that
    // function as its enclosing scope.
    bool saw_match = false;
    for (auto id : idents) {
        auto fn = enclosing_function(pipe.graph, id);
        if (fn.is_valid()) {
            EXPECT_EQ(fn, fns.front());
            saw_match = true;
        }
    }
    EXPECT_TRUE(saw_match);
}

TEST(CallGraph, MultiCallerEdgesCollected) {
    auto pipe = build_pipeline(Language::C, R"(
int helper(int x) { return x + 1; }
int caller_a(void) { return helper(1); }
int caller_b(void) { return helper(2); }
)");
    auto result = build_call_graph(pipe.graph, pipe.ast);
    EXPECT_GE(result.graph.edges.size(), 2U);
}

TEST(CallGraph, PythonAlsoWorks) {
    auto pipe = build_pipeline(Language::Python, R"(
def helper(x):
    return x + 1

def main():
    return helper(7)
)");
    auto result = build_call_graph(pipe.graph, pipe.ast);
    EXPECT_GT(result.stats.function_count, 0U);
    EXPECT_GT(result.stats.call_sites, 0U);
}

} // namespace
} // namespace euxis::reach
