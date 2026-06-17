#include <gtest/gtest.h>

#include "euxis/cpg/types.hpp"

namespace euxis::cpg {
namespace {

using euxis::parse::Language;

TEST(NodeId, InvalidByDefault) {
    NodeId id;
    EXPECT_FALSE(id.is_valid());
    EXPECT_EQ(id, kNullNode);
}

TEST(NodeId, EqualityIsValueBased) {
    EXPECT_EQ(NodeId{42}, NodeId{42});
    EXPECT_NE(NodeId{42}, NodeId{43});
}

TEST(NodeKind, StringifyCoversEnum) {
    EXPECT_STREQ(node_kind_str(NodeKind::Unknown),         "Unknown");
    EXPECT_STREQ(node_kind_str(NodeKind::TranslationUnit), "TranslationUnit");
    EXPECT_STREQ(node_kind_str(NodeKind::FunctionDef),     "FunctionDef");
    EXPECT_STREQ(node_kind_str(NodeKind::Call),            "Call");
    EXPECT_STREQ(node_kind_str(NodeKind::Identifier),      "Identifier");
    EXPECT_STREQ(node_kind_str(NodeKind::Literal),         "Literal");
    EXPECT_STREQ(node_kind_str(NodeKind::PreprocInclude),  "PreprocInclude");
}

TEST(EdgeKind, StringifyCoversEnum) {
    EXPECT_STREQ(edge_kind_str(EdgeKind::AstChild), "AstChild");
    EXPECT_STREQ(edge_kind_str(EdgeKind::Cfg),      "Cfg");
    EXPECT_STREQ(edge_kind_str(EdgeKind::Ddg),      "Ddg");
    EXPECT_STREQ(edge_kind_str(EdgeKind::Call),     "Call");
}

TEST(Classify, TranslationUnitVocabPerLanguage) {
    EXPECT_EQ(classify(Language::C,          "translation_unit"),   NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::Cpp,        "translation_unit"),   NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::Rust,       "source_file"),        NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::Go,         "source_file"),        NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::Python,     "module"),             NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::JavaScript, "program"),            NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::TypeScript, "program"),            NodeKind::TranslationUnit);
    EXPECT_EQ(classify(Language::Java,       "compilation_unit"),   NodeKind::TranslationUnit);
}

TEST(Classify, FunctionDefVocabPerLanguage) {
    EXPECT_EQ(classify(Language::C,          "function_definition"),   NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::Cpp,        "function_definition"),   NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::Rust,       "function_item"),         NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::Go,         "function_declaration"),  NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::Python,     "function_definition"),   NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::JavaScript, "function_declaration"),  NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::JavaScript, "arrow_function"),        NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::TypeScript, "method_definition"),     NodeKind::FunctionDef);
    EXPECT_EQ(classify(Language::Java,       "method_declaration"),    NodeKind::FunctionDef);
}

TEST(Classify, CommonVocabulary) {
    EXPECT_EQ(classify(Language::C,          "identifier"),       NodeKind::Identifier);
    EXPECT_EQ(classify(Language::Rust,       "identifier"),       NodeKind::Identifier);
    EXPECT_EQ(classify(Language::Cpp,        "call_expression"),  NodeKind::Call);
    EXPECT_EQ(classify(Language::Go,         "call_expression"),  NodeKind::Call);
    EXPECT_EQ(classify(Language::Python,     "return_statement"), NodeKind::Return);
    EXPECT_EQ(classify(Language::JavaScript, "if_statement"),     NodeKind::If);
    EXPECT_EQ(classify(Language::Java,       "if_statement"),     NodeKind::If);
}

TEST(Classify, LoopVocabulary) {
    EXPECT_EQ(classify(Language::C,          "for_statement"),         NodeKind::Loop);
    EXPECT_EQ(classify(Language::Cpp,        "while_statement"),       NodeKind::Loop);
    EXPECT_EQ(classify(Language::JavaScript, "for_in_statement"),      NodeKind::Loop);
    EXPECT_EQ(classify(Language::JavaScript, "for_of_statement"),      NodeKind::Loop);
    EXPECT_EQ(classify(Language::Java,       "enhanced_for_statement"), NodeKind::Loop);
}

TEST(Classify, ImportPerLanguage) {
    EXPECT_EQ(classify(Language::C,          "preproc_include"),       NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::Rust,       "use_declaration"),       NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::Go,         "import_declaration"),    NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::Python,     "import_statement"),      NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::Python,     "import_from_statement"), NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::JavaScript, "import_statement"),      NodeKind::PreprocInclude);
    EXPECT_EQ(classify(Language::Java,       "import_declaration"),    NodeKind::PreprocInclude);
}

TEST(Classify, UnknownVocab) {
    EXPECT_EQ(classify(Language::C, "totally_made_up_node"), NodeKind::Unknown);
    EXPECT_EQ(classify(Language::Rust, "xyz_macro"),         NodeKind::Unknown);
}

} // namespace
} // namespace euxis::cpg
