#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/cfg.hpp"
#include "euxis/cpg/ddg.hpp"
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

Graph build_pipeline(Language lang, const std::string& src,
                     bool include_cfg = false) {
    Parser p(lang);
    auto ast = p.parse(src);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto g = build(*ast);
    EXPECT_TRUE(g.has_value());
    if (include_cfg) build_cfg(*g);
    build_ddg(*g);
    return std::move(*g);
}

Graph build_pipeline_from_file(Language lang, const fs::path& path) {
    Parser p(lang);
    auto ast = p.parse_file(path);
    EXPECT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto g = build(*ast);
    EXPECT_TRUE(g.has_value());
    build_cfg(*g);
    build_ddg(*g);
    return std::move(*g);
}

std::size_t ddg_edge_count(const Graph& g) {
    std::size_t total = 0;
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Ddg) ++total;
    }
    return total;
}

bool has_ddg_edge(const Graph& g, NodeId from, NodeId to) {
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Ddg &&
            e.source == from && e.target == to) {
            return true;
        }
    }
    return false;
}

TEST(Ddg, EmptyGraphProducesNoStatsOrEdges) {
    Graph g;
    auto stats = build_ddg(g);
    EXPECT_EQ(stats.functions_processed,    0U);
    EXPECT_EQ(stats.definitions_recorded,   0U);
    EXPECT_EQ(stats.ddg_edges,              0U);
    EXPECT_EQ(stats.names_capped,           0U);
}

TEST(Ddg, StraightLineDefThenUseEmitsEdge) {
    auto g = build_pipeline(Language::C, R"(
int f(void) {
    int x = 1;
    return x;
}
)");
    // We expect at least one Ddg edge from the Declaration of `x`
    // to the use of `x` in the return.
    EXPECT_GT(ddg_edge_count(g), 0U);

    // Find the declarations and identifier uses.
    auto decls   = g.find_by_kind(NodeKind::Declaration);
    auto idents  = g.find_by_kind(NodeKind::Identifier);
    ASSERT_FALSE(decls.empty());
    ASSERT_FALSE(idents.empty());

    // At least one (decl, ident) pair must be connected.
    bool saw_def_to_use = false;
    for (NodeId d : decls) {
        for (NodeId u : idents) {
            if (has_ddg_edge(g, d, u)) {
                saw_def_to_use = true;
                break;
            }
        }
        if (saw_def_to_use) break;
    }
    EXPECT_TRUE(saw_def_to_use);
}

TEST(Ddg, MultipleUsesShareSameDef) {
    auto g = build_pipeline(Language::C, R"(
int f(void) {
    int x = 7;
    int a = x + 1;
    int b = x + 2;
    return a + b;
}
)");
    EXPECT_GT(ddg_edge_count(g), 1U);
}

TEST(Ddg, ReassignmentRedirectsLaterUses) {
    auto g = build_pipeline(Language::C, R"(
int f(void) {
    int x = 1;
    int a = x;
    x = 99;
    int b = x;
    return a + b;
}
)");
    // Both `a` and `b` should be defined; we won't try to map back
    // through the AST here, but the edge count should reflect more
    // than one def-use chain. Specifically:
    //   - decl(x=1) -> use of x in a = x
    //   - assignment(x=99) -> use of x in b = x
    //   - decl(a) -> use of a in return
    //   - decl(b) -> use of b in return
    EXPECT_GE(ddg_edge_count(g), 3U);
}

TEST(Ddg, AssignmentDoesNotCreateSelfDdgEdge) {
    auto g = build_pipeline(Language::C, R"(
int f(int n) {
    int x = 0;
    x = x + 1;
    return x;
}
)");
    // The walker must NOT emit a Ddg edge from the assignment
    // node to itself. Self-edges are explicitly suppressed in
    // emit_use().
    for (const auto& e : g.edges()) {
        if (e.kind == EdgeKind::Ddg) {
            EXPECT_NE(e.source, e.target)
                << "self Ddg edge at " << e.source.value;
        }
    }
}

TEST(Ddg, AssignmentRhsUsesPreviousDef) {
    auto g = build_pipeline(Language::C, R"(
int f(void) {
    int x = 5;
    x = x + 1;
    return x;
}
)");
    // The RHS `x` in `x = x + 1` must point at the Declaration of
    // x (the prior def), not at the Assignment node itself. We
    // can't easily match the specific RHS use here, but we can
    // verify that at least one Ddg edge originates from a
    // Declaration node (the original `int x = 5`).
    auto decls = g.find_by_kind(NodeKind::Declaration);
    ASSERT_FALSE(decls.empty());
    bool saw_decl_to_ident = false;
    for (NodeId d : decls) {
        for (const auto& e : g.edges()) {
            if (e.kind == EdgeKind::Ddg && e.source == d) {
                const Node* target = g.get(e.target);
                if (target != nullptr &&
                    target->kind == NodeKind::Identifier) {
                    saw_decl_to_ident = true;
                    break;
                }
            }
        }
        if (saw_decl_to_ident) break;
    }
    EXPECT_TRUE(saw_decl_to_ident);
}

TEST(Ddg, ParametersAreInitialBindings) {
    auto g = build_pipeline(Language::C, R"(
int f(int n) {
    return n + 1;
}
)");
    // The use of `n` in the return must Ddg-link back to a node
    // whose AstChild is a Parameter — confirming the parameter
    // contributed the initial binding.
    bool saw_param_origin = false;
    for (const auto& e : g.edges()) {
        if (e.kind != EdgeKind::Ddg) continue;
        const Node* src = g.get(e.source);
        if (src != nullptr && src->kind == NodeKind::Parameter) {
            saw_param_origin = true;
            break;
        }
    }
    EXPECT_TRUE(saw_param_origin);
}

TEST(Ddg, EachFunctionHasIsolatedScope) {
    auto g = build_pipeline(Language::C, R"(
int g(void) {
    int x = 1;
    return x;
}
int h(void) {
    int x = 2;
    return x;
}
)");
    // Neither function's Ddg edges may cross into the other's
    // scope. Because the def map resets per FunctionDef, this
    // is structurally guaranteed; assert no cross-function edge
    // exists.
    auto fns = g.find_by_kind(NodeKind::FunctionDef);
    ASSERT_GE(fns.size(), 2U);

    // Compute the source byte range of each function and assert
    // every Ddg edge's source and target sit inside the same
    // function.
    auto containing_fn = [&](NodeId node) -> NodeId {
        const Node* n = g.get(node);
        if (n == nullptr) return kNullNode;
        for (NodeId f : fns) {
            const Node* fn = g.get(f);
            if (fn == nullptr) continue;
            if (n->range.start_byte >= fn->range.start_byte &&
                n->range.end_byte   <= fn->range.end_byte) {
                return f;
            }
        }
        return kNullNode;
    };

    for (const auto& e : g.edges()) {
        if (e.kind != EdgeKind::Ddg) continue;
        EXPECT_EQ(containing_fn(e.source), containing_fn(e.target))
            << "Ddg edge crosses function boundary";
    }
}

TEST(Ddg, OptionsCanSuppressDeclarationTracking) {
    Parser p(Language::C);
    auto ast = p.parse(R"(
int f(void) {
    int x = 1;
    return x;
}
)");
    ASSERT_TRUE(ast.has_value());
    auto g = build(*ast);
    ASSERT_TRUE(g.has_value());

    DdgBuildOptions opts;
    opts.track_declarations = false;
    opts.track_assignments  = false;
    auto stats = build_ddg(*g, opts);
    // With both knobs off, parameters can still contribute defs,
    // but nothing inside the body. For a function without
    // parameters this means zero recorded defs and zero edges
    // from the body.
    EXPECT_EQ(stats.definitions_recorded, 0U);
}

TEST(Ddg, WorksOnPython) {
    auto g = build_pipeline(Language::Python, R"(
def f(n):
    x = 7
    y = x + n
    return y
)");
    EXPECT_GT(g.find_by_kind(NodeKind::FunctionDef).size(), 0U);
    EXPECT_GT(ddg_edge_count(g), 0U);
}

TEST(Ddg, WorksOnRust) {
    auto g = build_pipeline(Language::Rust, R"(
fn f(n: i32) -> i32 {
    let x = 7;
    let y = x + n;
    y
}
)");
    EXPECT_GT(g.find_by_kind(NodeKind::FunctionDef).size(), 0U);
    EXPECT_GT(ddg_edge_count(g), 0U);
}

TEST(Ddg, ProcessesEveryFixtureWithoutCrash) {
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
            << "no functions for " << path.string();
        // Some fixtures may legitimately have zero Ddg edges if
        // their FunctionDefs don't use any prior-defined names
        // (rare but possible for a one-liner). We only assert
        // the pipeline doesn't crash and produces *some* CPG
        // content, which earlier tests already cover; here we
        // just confirm the graph is non-empty.
        EXPECT_GT(g.node_count(), 5U);
    }
}

TEST(Ddg, StatsReflectMultipleFunctions) {
    auto g = build_pipeline(Language::C, R"(
int a(int n) { return n + 1; }
int b(int n) { return n + 2; }
int c(int n) { return n + 3; }
)", /*include_cfg=*/false);
    // Three FunctionDef walks; each fresh def map.
    Graph fresh = std::move(g);
    auto stats = build_ddg(fresh);
    EXPECT_GE(stats.functions_processed, 3U);
}

} // namespace
} // namespace euxis::cpg
