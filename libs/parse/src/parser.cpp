#include "euxis/parse/parser.hpp"

#include "euxis/parse/ast.hpp"

#include <tree_sitter/api.h>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

// Grammar entry points provided by the vendored tree-sitter grammar
// libraries. Each grammar exposes a single C ABI function that
// returns a pointer to the TSLanguage descriptor.
extern "C" {
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_cpp(void);
const TSLanguage* tree_sitter_rust(void);
const TSLanguage* tree_sitter_go(void);
const TSLanguage* tree_sitter_python(void);
const TSLanguage* tree_sitter_javascript(void);
const TSLanguage* tree_sitter_typescript(void);
const TSLanguage* tree_sitter_java(void);
}

namespace euxis::parse {

namespace {

const TSLanguage* ts_language_for(Language lang) noexcept {
    switch (lang) {
        case Language::C:          return tree_sitter_c();
        case Language::Cpp:        return tree_sitter_cpp();
        case Language::Rust:       return tree_sitter_rust();
        case Language::Go:         return tree_sitter_go();
        case Language::Python:     return tree_sitter_python();
        case Language::JavaScript: return tree_sitter_javascript();
        case Language::TypeScript: return tree_sitter_typescript();
        case Language::Java:       return tree_sitter_java();
    }
    return nullptr;
}

} // namespace

struct Parser::Impl {
    TSParser* parser{nullptr};

    Impl() = default;
    ~Impl() {
        if (parser != nullptr) ts_parser_delete(parser);
    }
    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept        = default;
    Impl& operator=(Impl&&) noexcept = default;
};

// Internal accessor — gives `parser.cpp` private access to Ast's
// PIMPL so it can wire the parsed tree + source into the returned
// handle without exposing Ast::Impl in the public header.
namespace detail {
struct AstAccess {
    static auto build(Language lang, TSTree* tree, std::string source) -> Ast {
        Ast a;
        a.impl_ = std::make_unique<Ast::Impl>();
        a.impl_->lang   = lang;
        a.impl_->tree   = tree;
        a.impl_->source = std::move(source);
        return a;
    }
};
} // namespace detail

Parser::Parser(Language lang)
    : impl_(std::make_unique<Impl>()),
      lang_(lang) {
    impl_->parser = ts_parser_new();
    const TSLanguage* tsl = ts_language_for(lang);
    if (tsl != nullptr) {
        ts_parser_set_language(impl_->parser, tsl);
    }
}

Parser::~Parser() = default;
Parser::Parser(Parser&&) noexcept = default;
Parser& Parser::operator=(Parser&&) noexcept = default;

auto Parser::language() const noexcept -> Language { return lang_; }

auto Parser::parse(std::string_view source)
    -> std::expected<Ast, ParseError> {
    if (impl_ == nullptr || impl_->parser == nullptr) {
        return std::unexpected(ParseError{
            .message = "Parser not initialised",
        });
    }

    // tree-sitter copies the input bytes internally during parse,
    // but the resulting tree carries byte offsets into the SAME
    // buffer the caller supplied. We need to keep a stable copy
    // alive for the Ast's lifetime, so we own a std::string here
    // and pass its bytes to ts_parser_parse_string.
    std::string owned{source};

    TSTree* tree = ts_parser_parse_string(
        impl_->parser,
        /*old_tree=*/nullptr,
        owned.data(),
        static_cast<std::uint32_t>(owned.size()));
    if (tree == nullptr) {
        return std::unexpected(ParseError{
            .message = "tree-sitter returned a null tree (parse timed out "
                       "or parser language not set)",
        });
    }
    return detail::AstAccess::build(lang_, tree, std::move(owned));
}

auto Parser::parse_file(const std::filesystem::path& path)
    -> std::expected<Ast, ParseError> {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::unexpected(ParseError{
            .message = "cannot open file",
            .file = path,
        });
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    auto out = parse(ss.str());
    if (!out) {
        auto err = out.error();
        err.file = path;
        return std::unexpected(std::move(err));
    }
    return out;
}

} // namespace euxis::parse
