#include "euxis/cli/tui/syntax.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// Language detection
// ---------------------------------------------------------------------------

Language detect_language(std::string_view hint) {
    // Normalize to lowercase
    std::string h;
    h.reserve(hint.size());
    for (char c : hint) h += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Remove leading dot
    if (!h.empty() && h[0] == '.') h = h.substr(1);

    if (h == "cpp" || h == "c++" || h == "cc" || h == "cxx" || h == "h" || h == "hpp" || h == "hxx") return Language::Cpp;
    if (h == "py" || h == "python") return Language::Python;
    if (h == "json") return Language::Json;
    if (h == "yaml" || h == "yml") return Language::Yaml;
    if (h == "bash" || h == "sh" || h == "zsh" || h == "fish" || h == "shell") return Language::Bash;
    if (h == "md" || h == "markdown") return Language::Markdown;
    if (h == "rs" || h == "rust") return Language::Rust;
    if (h == "ts" || h == "tsx" || h == "typescript" || h == "js" || h == "jsx" || h == "javascript") return Language::TypeScript;

    return Language::Unknown;
}

// ---------------------------------------------------------------------------
// Keyword sets
// ---------------------------------------------------------------------------

namespace {

const std::unordered_set<std::string>& cpp_keywords() {
    static const std::unordered_set<std::string> kw = {
        "auto", "break", "case", "catch", "class", "const", "constexpr", "continue",
        "default", "delete", "do", "else", "enum", "explicit", "export", "extern",
        "false", "for", "friend", "goto", "if", "inline", "namespace", "new",
        "noexcept", "nullptr", "operator", "override", "private", "protected", "public",
        "return", "sizeof", "static", "static_cast", "struct", "switch", "template",
        "this", "throw", "true", "try", "typedef", "typename", "using", "virtual",
        "void", "volatile", "while", "concept", "requires", "co_await", "co_return", "co_yield",
    };
    return kw;
}

const std::unordered_set<std::string>& cpp_types() {
    static const std::unordered_set<std::string> types = {
        "int", "char", "bool", "float", "double", "long", "short", "unsigned",
        "signed", "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "string", "vector", "map",
        "set", "unordered_map", "unordered_set", "shared_ptr", "unique_ptr",
        "optional", "variant", "span", "string_view",
    };
    return types;
}

const std::unordered_set<std::string>& python_keywords() {
    static const std::unordered_set<std::string> kw = {
        "and", "as", "assert", "async", "await", "break", "class", "continue",
        "def", "del", "elif", "else", "except", "False", "finally", "for",
        "from", "global", "if", "import", "in", "is", "lambda", "None",
        "nonlocal", "not", "or", "pass", "raise", "return", "True", "try",
        "while", "with", "yield", "match", "case", "type",
    };
    return kw;
}

const std::unordered_set<std::string>& rust_keywords() {
    static const std::unordered_set<std::string> kw = {
        "as", "async", "await", "break", "const", "continue", "crate", "dyn",
        "else", "enum", "extern", "false", "fn", "for", "if", "impl",
        "in", "let", "loop", "match", "mod", "move", "mut", "pub",
        "ref", "return", "self", "Self", "static", "struct", "super",
        "trait", "true", "type", "unsafe", "use", "where", "while",
    };
    return kw;
}

const std::unordered_set<std::string>& ts_keywords() {
    static const std::unordered_set<std::string> kw = {
        "abstract", "any", "as", "async", "await", "boolean", "break", "case",
        "catch", "class", "const", "continue", "debugger", "default", "delete",
        "do", "else", "enum", "export", "extends", "false", "finally", "for",
        "from", "function", "if", "implements", "import", "in", "instanceof",
        "interface", "let", "new", "null", "number", "of", "package",
        "private", "protected", "public", "return", "static", "string", "super",
        "switch", "this", "throw", "true", "try", "type", "typeof",
        "undefined", "var", "void", "while", "with", "yield",
    };
    return kw;
}

const std::unordered_set<std::string>& bash_keywords() {
    static const std::unordered_set<std::string> kw = {
        "if", "then", "else", "elif", "fi", "for", "while", "do", "done",
        "case", "esac", "function", "return", "local", "export", "source",
        "in", "select", "until", "break", "continue", "shift", "set", "unset",
    };
    return kw;
}

bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Extract a word token starting at position i
std::string extract_word(std::string_view line, size_t i) {
    size_t start = i;
    while (i < line.size() && is_word_char(line[i])) i++;
    return std::string(line.substr(start, i - start));
}

} // namespace

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

std::vector<Token> SyntaxHighlighter::tokenize(std::string_view line, Language lang) const {
    std::vector<Token> tokens;
    if (line.empty()) return tokens;

    const std::unordered_set<std::string>* keywords = nullptr;
    const std::unordered_set<std::string>* types = nullptr;
    std::string_view line_comment = "//";
    char string_chars[] = {'"', '\'', '\0'};
    bool has_preprocessor = false;

    switch (lang) {
        case Language::Cpp:
            keywords = &cpp_keywords();
            types = &cpp_types();
            has_preprocessor = true;
            break;
        case Language::Python:
            keywords = &python_keywords();
            line_comment = "#";
            break;
        case Language::Rust:
            keywords = &rust_keywords();
            break;
        case Language::TypeScript:
            keywords = &ts_keywords();
            break;
        case Language::Bash:
            keywords = &bash_keywords();
            line_comment = "#";
            break;
        case Language::Json:
            line_comment = ""; // No line comments in JSON
            break;
        case Language::Yaml:
            line_comment = "#";
            break;
        case Language::Markdown:
        case Language::Unknown:
            // Plain text, return single token
            tokens.push_back({0, line.size(), TokenType::Plain});
            return tokens;
    }

    size_t i = 0;
    while (i < line.size()) {
        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(line[i]))) {
            size_t start = i;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
            tokens.push_back({start, i - start, TokenType::Plain});
            continue;
        }

        // Line comment
        if (!line_comment.empty() && i + line_comment.size() <= line.size() &&
            line.substr(i, line_comment.size()) == line_comment) {
            tokens.push_back({i, line.size() - i, TokenType::Comment});
            break;
        }

        // Preprocessor
        if (has_preprocessor && line[i] == '#') {
            tokens.push_back({i, line.size() - i, TokenType::Preprocessor});
            break;
        }

        // String literals
        bool found_string = false;
        for (int si = 0; string_chars[si] != '\0'; ++si) {
            if (line[i] == string_chars[si]) {
                char quote = line[i];
                size_t start = i;
                i++;
                while (i < line.size() && line[i] != quote) {
                    if (line[i] == '\\' && i + 1 < line.size()) i++; // Skip escaped char
                    i++;
                }
                if (i < line.size()) i++; // Skip closing quote
                tokens.push_back({start, i - start, TokenType::String});
                found_string = true;
                break;
            }
        }
        if (found_string) continue;

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(line[i])) ||
            (line[i] == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t start = i;
            if (line[i] == '0' && i + 1 < line.size() && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                i += 2;
                while (i < line.size() && std::isxdigit(static_cast<unsigned char>(line[i]))) i++;
            } else {
                while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == 'e' || line[i] == 'E')) i++;
            }
            // Skip suffixes (f, u, l, etc.)
            while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i]))) i++;
            tokens.push_back({start, i - start, TokenType::Number});
            continue;
        }

        // Words (keywords, types, identifiers)
        if (is_word_char(line[i])) {
            size_t start = i;
            std::string word = extract_word(line, i);
            i = start + word.size();

            TokenType tt = TokenType::Plain;
            if (keywords && keywords->count(word)) tt = TokenType::Keyword;
            else if (types && types->count(word)) tt = TokenType::Type;
            // Check if followed by '(' -> function call
            else if (i < line.size() && line[i] == '(') tt = TokenType::Function;

            tokens.push_back({start, word.size(), tt});
            continue;
        }

        // Operators
        if (std::ispunct(static_cast<unsigned char>(line[i]))) {
            tokens.push_back({i, 1, TokenType::Operator});
            i++;
            continue;
        }

        // Fallback
        tokens.push_back({i, 1, TokenType::Plain});
        i++;
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// Token colors (Tokyo Dark palette)
// ---------------------------------------------------------------------------

RGB SyntaxHighlighter::token_color(TokenType type) {
    switch (type) {
        case TokenType::Keyword:      return {187, 154, 247}; // tokyo purple
        case TokenType::String:       return {158, 206, 106}; // tokyo green
        case TokenType::Number:       return {255, 158, 100}; // tokyo orange
        case TokenType::Comment:      return {68, 75, 106};   // tokyo dim (APCA-enforced separately)
        case TokenType::Operator:     return {137, 221, 255}; // tokyo cyan-bright
        case TokenType::Function:     return {125, 207, 255}; // tokyo light-blue
        case TokenType::Type:         return {42, 195, 222};  // tokyo teal
        case TokenType::Preprocessor: return {158, 206, 106}; // tokyo green
        case TokenType::Header:       return {255, 158, 100}; // tokyo orange
        case TokenType::Bold:         return {255, 255, 255}; // white
        case TokenType::Italic:       return {187, 154, 247}; // purple
        case TokenType::CodeInline:   return {158, 206, 106}; // green
        case TokenType::CodeBlock:    return {158, 206, 106}; // green
        case TokenType::List:         return {122, 162, 247}; // blue
        case TokenType::Link:         return {125, 207, 255}; // light blue
        case TokenType::Quote:        return {68, 75, 106};   // dim
        case TokenType::Plain:
        default:                      return {169, 177, 214}; // tokyo text
    }
}

} // namespace euxis::cli::tui
