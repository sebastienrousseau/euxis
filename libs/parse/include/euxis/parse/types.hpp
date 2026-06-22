/// @file
/// @brief Public types for libs/parse.
///
/// The Parser/Ast pair is the foundation for every C++23 facility
/// that needs a syntax tree: the Code Property Graph (P1.3), the
/// taint engine (P1.5), the slopsquatting follow-on for #include
/// directives, the reachability pruner (P1.7), and any future
/// rule-pack ingester that wants to ask "does this AST contain
/// pattern X?".
///
/// libs/parse intentionally hides tree-sitter from its public
/// surface. Every consumer talks to Parser / Ast / Language /
/// SourceRange — none of those headers transitively include
/// `<tree_sitter/api.h>`. That isolates the rest of the C++23
/// codebase from tree-sitter ABI churn and lets us swap the
/// backend (libclang, lezer, hand-rolled) later without touching
/// the call sites.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace euxis::parse {

/// Languages with a wired grammar. Add a new enumerator when a new
/// tree-sitter grammar is fetched in libs/parse/CMakeLists.txt.
enum class Language : std::uint8_t {
    C,
    Cpp,
    Rust,
    Go,
    Python,
    JavaScript,
    TypeScript,
    Java,
};

/// Stringify for diagnostics + Finding emission.
[[nodiscard]] auto language_str(Language lang) noexcept -> const char*;

/// Detect a Language from a file extension. Returns std::nullopt for
/// unknown extensions so callers can degrade gracefully (e.g. skip
/// the file rather than treating it as C).
[[nodiscard]] auto detect_language(const std::filesystem::path& path)
    -> std::optional<Language>;

/// Detect a Language from a file extension string. The leading dot
/// is optional. Useful when the caller has the extension in hand
/// without a real path.
[[nodiscard]] auto detect_language_by_extension(std::string_view ext)
    -> std::optional<Language>;

/// Source-code byte range. All offsets are zero-based. `row` /
/// `column` are 0-based to match tree-sitter; SARIF emitters
/// elsewhere in the codebase convert to 1-based at the boundary.
struct SourceRange {
    std::uint32_t start_row{0};
    std::uint32_t start_column{0};
    std::uint32_t end_row{0};
    std::uint32_t end_column{0};
    std::uint32_t start_byte{0};
    std::uint32_t end_byte{0};
};

/// Parser error.
struct ParseError {
    std::string message;
    std::filesystem::path file;
};

} // namespace euxis::parse
