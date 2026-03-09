#pragma once

#include "euxis/cli/tui/color_system.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace euxis::cli::tui {

/// Token types for syntax highlighting.
enum class TokenType {
    Plain,
    Keyword,
    String,
    Number,
    Comment,
    Operator,
    Function,
    Type,
    Preprocessor,
    // Markdown-specific
    Header,
    Bold,
    Italic,
    CodeInline,
    CodeBlock,
    List,
    Link,
    Quote,
};

/// A single syntax token with position and type.
struct Token {
    size_t start{0};
    size_t length{0};
    TokenType type{TokenType::Plain};
};

/// Supported languages for syntax highlighting.
enum class Language {
    Unknown,
    Cpp,
    Python,
    Json,
    Yaml,
    Bash,
    Markdown,
    Rust,
    TypeScript,
};

/// Detect language from filename extension or fenced code block tag.
[[nodiscard]] Language detect_language(std::string_view hint);

/// Regex-based syntax tokenizer.
class SyntaxHighlighter {
public:
    /// Tokenize a single line of code.
    [[nodiscard]] std::vector<Token> tokenize(std::string_view line, Language lang) const;

    /// Get the default color for a token type (Tokyo Dark palette).
    [[nodiscard]] static RGB token_color(TokenType type);
};

} // namespace euxis::cli::tui
