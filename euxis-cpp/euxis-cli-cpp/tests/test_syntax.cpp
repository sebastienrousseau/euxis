#include <gtest/gtest.h>
#include "euxis/cli/tui/syntax.hpp"

namespace euxis::cli::tui {
namespace {

// --- Language Detection ---

TEST(LanguageTest, DetectCpp) {
    EXPECT_EQ(detect_language("cpp"), Language::Cpp);
    EXPECT_EQ(detect_language("c++"), Language::Cpp);
    EXPECT_EQ(detect_language(".hpp"), Language::Cpp);
    EXPECT_EQ(detect_language("h"), Language::Cpp);
}

TEST(LanguageTest, DetectPython) {
    EXPECT_EQ(detect_language("py"), Language::Python);
    EXPECT_EQ(detect_language("python"), Language::Python);
}

TEST(LanguageTest, DetectJson) {
    EXPECT_EQ(detect_language("json"), Language::Json);
}

TEST(LanguageTest, DetectYaml) {
    EXPECT_EQ(detect_language("yaml"), Language::Yaml);
    EXPECT_EQ(detect_language("yml"), Language::Yaml);
}

TEST(LanguageTest, DetectBash) {
    EXPECT_EQ(detect_language("bash"), Language::Bash);
    EXPECT_EQ(detect_language("sh"), Language::Bash);
    EXPECT_EQ(detect_language("zsh"), Language::Bash);
    EXPECT_EQ(detect_language("fish"), Language::Bash);
}

TEST(LanguageTest, DetectMarkdown) {
    EXPECT_EQ(detect_language("md"), Language::Markdown);
    EXPECT_EQ(detect_language("markdown"), Language::Markdown);
}

TEST(LanguageTest, DetectRust) {
    EXPECT_EQ(detect_language("rs"), Language::Rust);
    EXPECT_EQ(detect_language("rust"), Language::Rust);
}

TEST(LanguageTest, DetectTypeScript) {
    EXPECT_EQ(detect_language("ts"), Language::TypeScript);
    EXPECT_EQ(detect_language("tsx"), Language::TypeScript);
    EXPECT_EQ(detect_language("js"), Language::TypeScript);
}

TEST(LanguageTest, DetectUnknown) {
    EXPECT_EQ(detect_language("xyz"), Language::Unknown);
    EXPECT_EQ(detect_language(""), Language::Unknown);
}

TEST(LanguageTest, CaseInsensitive) {
    EXPECT_EQ(detect_language("CPP"), Language::Cpp);
    EXPECT_EQ(detect_language("PY"), Language::Python);
}

// --- Tokenizer Tests ---

TEST(SyntaxTest, EmptyLine) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("", Language::Cpp);
    EXPECT_TRUE(tokens.empty());
}

TEST(SyntaxTest, CppKeywords) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("if (true) return;", Language::Cpp);

    bool found_keyword = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Keyword) {
            found_keyword = true;
            break;
        }
    }
    EXPECT_TRUE(found_keyword);
}

TEST(SyntaxTest, CppString) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize(R"(auto s = "hello world";)", Language::Cpp);

    bool found_string = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::String) {
            found_string = true;
            break;
        }
    }
    EXPECT_TRUE(found_string);
}

TEST(SyntaxTest, CppComment) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("int x; // comment", Language::Cpp);

    bool found_comment = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Comment) {
            found_comment = true;
            break;
        }
    }
    EXPECT_TRUE(found_comment);
}

TEST(SyntaxTest, CppNumber) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("int x = 42;", Language::Cpp);

    bool found_number = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Number) {
            found_number = true;
            break;
        }
    }
    EXPECT_TRUE(found_number);
}

TEST(SyntaxTest, CppHexNumber) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("int x = 0xFF;", Language::Cpp);

    bool found_number = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Number) {
            found_number = true;
            break;
        }
    }
    EXPECT_TRUE(found_number);
}

TEST(SyntaxTest, CppPreprocessor) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("#include <iostream>", Language::Cpp);

    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens[0].type, TokenType::Preprocessor);
}

TEST(SyntaxTest, CppType) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("int x = 0;", Language::Cpp);

    bool found_type = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Type) {
            found_type = true;
            break;
        }
    }
    EXPECT_TRUE(found_type);
}

TEST(SyntaxTest, CppFunction) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("foo(bar);", Language::Cpp);

    bool found_func = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Function) {
            found_func = true;
            break;
        }
    }
    EXPECT_TRUE(found_func);
}

TEST(SyntaxTest, PythonComment) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("x = 1  # comment", Language::Python);

    bool found_comment = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Comment) {
            found_comment = true;
            break;
        }
    }
    EXPECT_TRUE(found_comment);
}

TEST(SyntaxTest, PythonKeywords) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("def foo():", Language::Python);

    bool found_keyword = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Keyword) {
            found_keyword = true;
            break;
        }
    }
    EXPECT_TRUE(found_keyword);
}

TEST(SyntaxTest, RustKeywords) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("fn main() {}", Language::Rust);

    bool found_keyword = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Keyword) {
            found_keyword = true;
            break;
        }
    }
    EXPECT_TRUE(found_keyword);
}

TEST(SyntaxTest, BashComment) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("echo hello # world", Language::Bash);

    bool found_comment = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Comment) {
            found_comment = true;
            break;
        }
    }
    EXPECT_TRUE(found_comment);
}

TEST(SyntaxTest, MarkdownIsPlainText) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("# Hello **world**", Language::Markdown);

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::Plain);
}

TEST(SyntaxTest, UnknownLanguageIsPlainText) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize("anything goes here", Language::Unknown);

    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::Plain);
}

TEST(SyntaxTest, TokenColorNonZero) {
    auto c = SyntaxHighlighter::token_color(TokenType::Keyword);
    EXPECT_GT(c.r + c.g + c.b, 0);
}

TEST(SyntaxTest, AllTokenTypesHaveColors) {
    for (int i = 0; i <= static_cast<int>(TokenType::Preprocessor); ++i) {
        auto c = SyntaxHighlighter::token_color(static_cast<TokenType>(i));
        EXPECT_GT(c.r + c.g + c.b, 0) << "TokenType " << i << " has zero color";
    }
}

TEST(SyntaxTest, EscapedStringContent) {
    SyntaxHighlighter sh;
    auto tokens = sh.tokenize(R"(auto s = "hello \"world\"";)", Language::Cpp);

    int string_count = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::String) string_count++;
    }
    EXPECT_EQ(string_count, 1); // Should be one string, not fragmented
}

TEST(SyntaxTest, TokensCoverFullLine) {
    SyntaxHighlighter sh;
    std::string_view line = "int x = 42;";
    auto tokens = sh.tokenize(line, Language::Cpp);

    size_t covered = 0;
    for (const auto& t : tokens) covered += t.length;
    EXPECT_EQ(covered, line.size());
}

} // namespace
} // namespace euxis::cli::tui
