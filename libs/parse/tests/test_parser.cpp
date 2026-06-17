#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "euxis/parse/ast.hpp"
#include "euxis/parse/parser.hpp"

#ifndef EUXIS_PARSE_FIXTURES_DIR
#define EUXIS_PARSE_FIXTURES_DIR "."
#endif

namespace euxis::parse {
namespace {

namespace fs = std::filesystem;

fs::path fixtures_dir() {
    return fs::path{EUXIS_PARSE_FIXTURES_DIR};
}

TEST(Parser, ParsesMinimalCBuffer) {
    Parser p(Language::C);
    auto ast = p.parse("int main(void) { return 0; }");
    ASSERT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    EXPECT_EQ(ast->language(), Language::C);
    EXPECT_FALSE(ast->has_errors());
    EXPECT_EQ(ast->root_kind(), "translation_unit");
    EXPECT_GT(ast->root_named_child_count(), 0U);
}

TEST(Parser, ParsesMinimalCppBuffer) {
    Parser p(Language::Cpp);
    auto ast = p.parse("namespace x { int f() { return 0; } }");
    ASSERT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    EXPECT_EQ(ast->language(), Language::Cpp);
    EXPECT_FALSE(ast->has_errors());
    EXPECT_EQ(ast->root_kind(), "translation_unit");
    EXPECT_GT(ast->root_named_child_count(), 0U);
}

TEST(Parser, ParsesCFile) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixtures_dir() / "c" / "hello.c");
    ASSERT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    EXPECT_FALSE(ast->has_errors());
    EXPECT_GT(ast->count_named_nodes(), 5U);
}

TEST(Parser, ParsesCppFile) {
    Parser p(Language::Cpp);
    auto ast = p.parse_file(fixtures_dir() / "cpp" / "hello.cpp");
    ASSERT_TRUE(ast.has_value()) << (ast ? "" : ast.error().message);
    EXPECT_FALSE(ast->has_errors());
    EXPECT_GT(ast->count_named_nodes(), 10U);
}

TEST(Parser, DetectsSyntaxErrorWithoutFailing) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixtures_dir() / "c" / "syntax_error.c");
    // The unexpected branch is reserved for I/O; a partial AST with
    // syntax errors is still wrapped in the success branch.
    ASSERT_TRUE(ast.has_value());
    EXPECT_TRUE(ast->has_errors());
}

TEST(Parser, MissingFileSurfacesError) {
    Parser p(Language::Cpp);
    auto ast = p.parse_file(fixtures_dir() / "does-not-exist.cpp");
    EXPECT_FALSE(ast.has_value());
}

TEST(Parser, AstOwnsSourceBytes) {
    Parser p(Language::C);
    auto ast = p.parse(std::string{"int x = 42;"});
    ASSERT_TRUE(ast.has_value());
    EXPECT_EQ(ast->source(), "int x = 42;");
}

TEST(Parser, RootRangeCoversWholeSource) {
    Parser p(Language::Cpp);
    auto ast = p.parse(std::string{"int x = 0;"});
    ASSERT_TRUE(ast.has_value());
    auto range = ast->root_range();
    EXPECT_EQ(range.start_byte, 0U);
    EXPECT_EQ(range.end_byte,   ast->source().size());
    EXPECT_EQ(range.start_row,  0U);
}

TEST(Parser, NodeTextRoundTrip) {
    Parser p(Language::C);
    auto ast = p.parse(std::string{"int main(void) { return 0; }"});
    ASSERT_TRUE(ast.has_value());
    auto range = ast->root_range();
    EXPECT_EQ(ast->node_text(range), ast->source());
}

TEST(Parser, VisitorSeesNamedNodes) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixtures_dir() / "c" / "hello.c");
    ASSERT_TRUE(ast.has_value());

    std::vector<std::string> kinds;
    ast->visit_named_nodes(
        [&](std::string_view kind, SourceRange /*range*/) {
            kinds.emplace_back(kind);
            return true;
        });
    ASSERT_FALSE(kinds.empty());

    bool saw_function = false;
    bool saw_include  = false;
    for (const auto& k : kinds) {
        if (k == "function_definition") saw_function = true;
        if (k == "preproc_include")     saw_include  = true;
    }
    EXPECT_TRUE(saw_function);
    EXPECT_TRUE(saw_include);
}

TEST(Parser, VisitorCanShortCircuitDescent) {
    Parser p(Language::C);
    auto ast = p.parse_file(fixtures_dir() / "c" / "hello.c");
    ASSERT_TRUE(ast.has_value());

    std::size_t shallow_count = 0;
    ast->visit_named_nodes(
        [&](std::string_view, SourceRange) {
            ++shallow_count;
            return false;  // do not descend into any node
        });
    auto full_count = ast->count_named_nodes();
    EXPECT_LT(shallow_count, full_count);
    EXPECT_GT(shallow_count, 0U);
}

TEST(Parser, MoveConstructorTransfersOwnership) {
    Parser p(Language::Cpp);
    auto first = p.parse(std::string{"int x = 1;"});
    ASSERT_TRUE(first.has_value());
    Ast moved{std::move(*first)};
    EXPECT_EQ(moved.root_kind(), "translation_unit");
    EXPECT_EQ(moved.source(), "int x = 1;");
}

} // namespace
} // namespace euxis::parse
