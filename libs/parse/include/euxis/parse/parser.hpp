/// @file
/// @brief Tree-sitter-backed parser facade.
///
/// One `Parser` per language. Move-only because the underlying
/// tree-sitter `TSParser*` is heavyweight. Re-use the same parser
/// across many parse calls — tree-sitter recommends this pattern
/// because the parser state is significantly more expensive to
/// initialise than to reuse.
///
/// `parse()` and `parse_file()` both return `std::expected<Ast,
/// ParseError>` so callers can branch on success without exceptions.
/// A parse that produced an AST with syntax errors is NOT an error
/// for the purposes of this API — the Ast is returned and the caller
/// inspects `ast.has_errors()` to decide what to do with the partial
/// tree. The unexpected branch is reserved for I/O failures and
/// parser initialisation issues.
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>

#include "euxis/parse/ast.hpp"
#include "euxis/parse/types.hpp"

namespace euxis::parse {

class Parser {
public:
    explicit Parser(Language lang);
    ~Parser();

    Parser(const Parser&)            = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) noexcept;
    Parser& operator=(Parser&&) noexcept;

    /// Language the parser was constructed for.
    [[nodiscard]] auto language() const noexcept -> Language;

    /// Parse a buffer of source bytes. The returned Ast owns a copy
    /// of `source`. Returns ParseError only if the parser timed out
    /// or could not be initialised; a partial AST with
    /// `has_errors()==true` is still wrapped in the success branch.
    [[nodiscard]] auto parse(std::string_view source)
        -> std::expected<Ast, ParseError>;

    /// Parse the contents of `path`. Convenience over `parse()` —
    /// reads the file, dispatches to the buffer overload. Errors
    /// from file I/O appear in the unexpected branch.
    [[nodiscard]] auto parse_file(const std::filesystem::path& path)
        -> std::expected<Ast, ParseError>;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Language lang_;
};

} // namespace euxis::parse
