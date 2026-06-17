#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/cfg.hpp"
#include "euxis/cpg/graph.hpp"
#include "euxis/cpg/types.hpp"
#include "euxis/parse/parser.hpp"

#ifndef EUXIS_PARSE_FIXTURES_DIR
#define EUXIS_PARSE_FIXTURES_DIR "."
#endif

namespace euxis::cpg {
namespace {

namespace fs = std::filesystem;

using euxis::parse::Language;
using euxis::parse::Parser;

fs::path fixture(const char* lang, const char* file) {
    return fs::path{EUXIS_PARSE_FIXTURES_DIR} / lang / file;
}

/// Build the AST + CPG + CFG for a buffer of source. Returns the
/// fully-populated Graph; tests then inspect the Cfg edges.
Graph build_pipeline(Language lang, const std::string& source) {
    Parser p(lang);
    auto ast = p.parse(source);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto graph = build(*ast);
    EXPECT_TRUE(graph.has_value());
    build_cfg(*graph);
    return std::move(*graph);
}

Graph build_pipeline_from_file(Language lang, const fs::path& path) {
    Parser p(lang);
    auto ast = p.parse_file(path);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto graph = build(*ast);
    EXPECT_TRUE(graph.has_value());
    build_cfg(*graph);
    return std::move(*graph);
}

std::size_t cfg_edge_count(const Graph& g) {
    std::size_t total = 0;
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Cfg) ++total;
    }
    return total;
}

bool has_cfg_edge(const Graph& g, NodeId from, NodeId to) {
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Cfg &&
            e.source == from && e.target == to) {
            return true;
        }
    }
    return false;
}

TEST(Cfg, EmptyGraphProducesNoStatsOrEdges) {
    Graph g;
    auto stats = build_cfg(g);
    EXPECT_EQ(stats.functions_processed, 0U);
    EXPECT_EQ(stats.sequential_edges,    0U);
    EXPECT_EQ(stats.branch_entry_edges,  0U);
    EXPECT_EQ(stats.loop_back_edges,     0U);
}

TEST(Cfg, StraightLineFunctionGetsSequentialEdges) {
    auto g = build_pipeline(Language::C, R"(
int main(void) {
    int a = 1;
    int b = 2;
    int c = 3;
    return c;
}
)");
    auto main_fns = g.find_by_kind(NodeKind::FunctionDef);
    ASSERT_FALSE(main_fns.empty());

    // The body has 4 direct children (3 declarations + 1 return),
    // giving at least 3 sequential edges.
    EXPECT_GE(cfg_edge_count(g), 3U);

    // Build was processed.
    Graph fresh;
    auto stats = build_cfg(fresh);
    EXPECT_EQ(stats.functions_processed, 0U);
}

TEST(Cfg, StatsReportsFunctionsProcessed) {
    auto g = build_pipeline(Language::C, R"(
int add(int a, int b) { return a + b; }
int main(void) { return add(1, 2); }
)");
    Graph fresh = std::move(g);
    auto stats = build_cfg(fresh);
    // Calling build_cfg again on the same graph re-runs over every
    // FunctionDef; the stats report the second pass count, which
    // still covers every function present.
    EXPECT_GE(stats.functions_processed, 2U);
}

TEST(Cfg, IfBranchEntryEdges) {
    auto g = build_pipeline(Language::C, R"(
int branch(int x) {
    if (x > 0) {
        return 1;
    } else {
        return 0;
    }
}
)");

    auto ifs = g.find_by_kind(NodeKind::If);
    ASSERT_FALSE(ifs.empty()) << "expected at least one If node";

    // The If should have at least one branch-entry Cfg edge.
    bool saw_outgoing = false;
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Cfg && e.source == ifs.front()) {
            saw_outgoing = true;
            break;
        }
    }
    EXPECT_TRUE(saw_outgoing);
}

TEST(Cfg, LoopBackEdgeFromBodyLastToHeader) {
    auto g = build_pipeline(Language::C, R"(
int sum(int n) {
    int total = 0;
    while (n > 0) {
        total = total + n;
        n = n - 1;
    }
    return total;
}
)");

    auto loops = g.find_by_kind(NodeKind::Loop);
    ASSERT_FALSE(loops.empty());

    // At least one Cfg edge should target the Loop node — that's
    // the back-edge from the last body statement.
    bool saw_back_edge = false;
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Cfg && e.target == loops.front() &&
            e.source != loops.front()) {
            saw_back_edge = true;
            break;
        }
    }
    EXPECT_TRUE(saw_back_edge);
}

TEST(Cfg, ConsecutiveSiblingsConnectedInOrder) {
    auto g = build_pipeline(Language::C, R"(
int main(void) {
    int a = 1;
    int b = 2;
    return b;
}
)");
    // Find the body Block — first Block descendant of the main
    // FunctionDef.
    auto fns = g.find_by_kind(NodeKind::FunctionDef);
    ASSERT_FALSE(fns.empty());

    auto blocks = g.find_by_kind(NodeKind::Block);
    ASSERT_FALSE(blocks.empty());

    NodeId body = blocks.front();
    auto children = g.children(body);
    ASSERT_GE(children.size(), 2U);

    // Each consecutive pair should be connected.
    for (std::size_t i = 0; i + 1 < children.size(); ++i) {
        EXPECT_TRUE(has_cfg_edge(g, children[i], children[i + 1]))
            << "missing sequential edge from "
            << children[i].value << " to " << children[i + 1].value;
    }
}

TEST(Cfg, RebuildIsIdempotentOnStats) {
    // Building CFG twice doubles the edges (we don't dedupe), but
    // the stats reported by the SECOND call should still describe a
    // full pass — primarily a check that the builder doesn't crash
    // or silently skip on a graph that already has Cfg edges.
    auto g = build_pipeline(Language::C,
        "int f(void) { int x = 1; return x; }");
    auto edges_before = cfg_edge_count(g);
    auto stats = build_cfg(g);
    auto edges_after = cfg_edge_count(g);
    EXPECT_GE(stats.functions_processed, 1U);
    EXPECT_EQ(edges_after, edges_before + stats.sequential_edges +
                                  stats.branch_entry_edges +
                                  stats.loop_back_edges);
}

TEST(Cfg, OptionsCanSuppressBranchEdges) {
    Parser p(Language::C);
    auto ast = p.parse(R"(
int f(int x) {
    if (x) { return 1; } else { return 0; }
}
)");
    ASSERT_TRUE(ast.has_value());
    auto g = build(*ast);
    ASSERT_TRUE(g.has_value());

    CfgBuildOptions opts;
    opts.emit_if_branch_edges = false;
    opts.emit_loop_back_edges = false;
    auto stats = build_cfg(*g, opts);
    EXPECT_EQ(stats.branch_entry_edges, 0U);
    EXPECT_EQ(stats.loop_back_edges,    0U);
    EXPECT_GE(stats.sequential_edges,   0U);
}

TEST(Cfg, WorksAcrossLanguagesPython) {
    auto g = build_pipeline(Language::Python, R"(
def f(x):
    a = 1
    b = 2
    return a + b + x
)");
    auto fns = g.find_by_kind(NodeKind::FunctionDef);
    ASSERT_FALSE(fns.empty());
    EXPECT_GT(cfg_edge_count(g), 0U);
}

TEST(Cfg, WorksAcrossLanguagesRust) {
    auto g = build_pipeline(Language::Rust, R"(
fn f(x: i32) -> i32 {
    let a = 1;
    let b = 2;
    a + b + x
}
)");
    auto fns = g.find_by_kind(NodeKind::FunctionDef);
    ASSERT_FALSE(fns.empty());
    EXPECT_GT(cfg_edge_count(g), 0U);
}

TEST(Cfg, ProcessesEveryFixtureWithoutCrash) {
    const std::pair<Language, fs::path> cases[] = {
        {Language::C,          fixture("c",          "hello.c")},
        {Language::Cpp,        fixture("cpp",        "hello.cpp")},
        {Language::Rust,       fixture("rust",       "hello.rs")},
        {Language::Go,         fixture("go",         "hello.go")},
        {Language::Python,     fixture("python",     "hello.py")},
        {Language::JavaScript, fixture("javascript", "hello.js")},
        {Language::TypeScript, fixture("typescript", "hello.ts")},
        {Language::Java,       fixture("java",       "Hello.java")},
    };
    for (const auto& [lang, path] : cases) {
        auto g = build_pipeline_from_file(lang, path);
        EXPECT_GT(g.find_by_kind(NodeKind::FunctionDef).size(), 0U)
            << "no functions found for " << path.string();
        EXPECT_GT(cfg_edge_count(g), 0U)
            << "no CFG edges built for " << path.string();
    }
}

} // namespace
} // namespace euxis::cpg
