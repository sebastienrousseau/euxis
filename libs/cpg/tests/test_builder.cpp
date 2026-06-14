#include <gtest/gtest.h>

#include <filesystem>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/types.hpp"
#include "euxis/parse/parser.hpp"

namespace euxis::cpg {
namespace {

namespace fs = std::filesystem;

using euxis::parse::Language;
using euxis::parse::Parser;

// EUXIS_PARSE_FIXTURES_DIR is defined by libs/parse/CMakeLists.txt.
// libs/cpg's tests pull from the same directory so we don't fork
// the corpus and keep two paths in sync.
#ifndef EUXIS_PARSE_FIXTURES_DIR
#define EUXIS_PARSE_FIXTURES_DIR "."
#endif

fs::path fixture(const char* lang, const char* file) {
    return fs::path{EUXIS_PARSE_FIXTURES_DIR} / lang / file;
}

TEST(Builder, EmptyBufferProducesEmptyGraph) {
    Parser p(Language::C);
    auto ast = p.parse("");
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    // An empty Ast still has a translation_unit root; the visitor
    // emits one node for it.
    EXPECT_LE(graph->node_count(), 1U);
}

TEST(Builder, CFileProducesPopulatedGraph) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixture("c", "hello.c"));
    ASSERT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    EXPECT_GT(graph->node_count(), 5U);
    EXPECT_GT(graph->edge_count(), 4U);
}

TEST(Builder, RootIsTranslationUnit) {
    Parser p(Language::Cpp);
    auto ast = p.parse_file(fixture("cpp", "hello.cpp"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    const Node* root = graph->get(graph->root());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind, NodeKind::TranslationUnit);
}

TEST(Builder, FunctionDefsLocated) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixture("c", "hello.c"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    auto fns = graph->find_by_kind(NodeKind::FunctionDef);
    EXPECT_GE(fns.size(), 2U)  // `add` and `main`
        << "Expected at least add() and main() in hello.c";
}

TEST(Builder, IdentifiersCarryName) {
    Parser p(Language::C);
    auto ast = p.parse("int x = 42;");
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    auto idents = graph->find_by_kind(NodeKind::Identifier);
    ASSERT_FALSE(idents.empty());
    const Node* n = graph->get(idents.front());
    ASSERT_NE(n, nullptr);
    EXPECT_FALSE(n->name.empty());
    EXPECT_EQ(n->name, "x");
}

TEST(Builder, PreprocIncludeRecognised) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixture("c", "hello.c"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    auto incs = graph->find_by_kind(NodeKind::PreprocInclude);
    EXPECT_GE(incs.size(), 1U);  // <stdio.h>
}

TEST(Builder, AstChildEdgesFormSpanningTree) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixture("c", "hello.c"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());

    // Every node except the root must have exactly one AstChild
    // parent. (The CPG is a tree on AstChild edges; CFG / DDG
    // edges will add other parentage in later batches.)
    for (const auto& n : graph->nodes()) {
        if (n.id == graph->root()) continue;
        EXPECT_NE(graph->parent(n.id), kNullNode)
            << "node id=" << n.id.value
            << " kind=" << node_kind_str(n.kind)
            << " has no AstChild parent";
    }
}

TEST(Builder, NodeLanguagePreservedAcrossGrammars) {
    Parser p_cpp(Language::Cpp);
    auto ast_cpp = p_cpp.parse_file(fixture("cpp", "hello.cpp"));
    ASSERT_TRUE(ast_cpp.has_value());
    auto g_cpp = build(*ast_cpp);
    ASSERT_TRUE(g_cpp.has_value());
    for (const auto& n : g_cpp->nodes()) {
        EXPECT_EQ(n.language, Language::Cpp);
    }

    Parser p_rust(Language::Rust);
    auto ast_rust = p_rust.parse_file(fixture("rust", "hello.rs"));
    ASSERT_TRUE(ast_rust.has_value());
    auto g_rust = build(*ast_rust);
    ASSERT_TRUE(g_rust.has_value());
    auto rust_fns = g_rust->find_by_kind(NodeKind::FunctionDef);
    EXPECT_GE(rust_fns.size(), 2U);  // add + main
}

TEST(Builder, PythonFunctionAndImport) {
    Parser p(Language::Python);
    auto ast = p.parse_file(fixture("python", "hello.py"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    EXPECT_GE(graph->find_by_kind(NodeKind::FunctionDef).size(), 1U);
    EXPECT_GE(graph->find_by_kind(NodeKind::PreprocInclude).size(), 1U);
}

TEST(Builder, GoFunctionAndImport) {
    Parser p(Language::Go);
    auto ast = p.parse_file(fixture("go", "hello.go"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    EXPECT_GE(graph->find_by_kind(NodeKind::FunctionDef).size(), 1U);
    EXPECT_GE(graph->find_by_kind(NodeKind::PreprocInclude).size(), 1U);
}

TEST(Builder, JavaMethodDeclarationRecognised) {
    Parser p(Language::Java);
    auto ast = p.parse_file(fixture("java", "Hello.java"));
    ASSERT_TRUE(ast.has_value());
    auto graph = build(*ast);
    ASSERT_TRUE(graph.has_value());
    auto methods = graph->find_by_kind(NodeKind::FunctionDef);
    EXPECT_GE(methods.size(), 2U)
        << "Expected at least add() and make() and main() to surface as FunctionDef";
}

TEST(Builder, AstChildOrderIsStable) {
    Parser p(Language::C);
    auto a = p.parse_file(fixture("c", "hello.c"));
    auto b = p.parse_file(fixture("c", "hello.c"));
    ASSERT_TRUE(a.has_value() && b.has_value());
    auto ga = build(*a);
    auto gb = build(*b);
    ASSERT_TRUE(ga.has_value() && gb.has_value());

    EXPECT_EQ(ga->node_count(), gb->node_count());
    EXPECT_EQ(ga->edge_count(), gb->edge_count());

    auto na = ga->nodes();
    auto nb = gb->nodes();
    for (std::size_t i = 0; i < na.size(); ++i) {
        EXPECT_EQ(na[i].kind,        nb[i].kind);
        EXPECT_EQ(na[i].raw_kind,    nb[i].raw_kind);
        EXPECT_EQ(na[i].range.start_byte, nb[i].range.start_byte);
    }
}

} // namespace
} // namespace euxis::cpg
