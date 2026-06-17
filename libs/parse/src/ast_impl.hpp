/// @file
/// @brief Internal definition of `Ast::Impl` for use by both
///        `ast.cpp` and `parser.cpp`. Not part of the public API.
///
/// `parser.cpp` constructs an `Ast` and pokes its private impl via
/// the friended `detail::AstAccess` helper; doing so requires the
/// complete type of `Ast::Impl`, which previously lived inline in
/// `ast.cpp` and was invisible to `parser.cpp`. This header is the
/// shared definition so both translation units see the same Impl.
#pragma once

#include "euxis/parse/ast.hpp"

#include <tree_sitter/api.h>

#include <string>

namespace euxis::parse {

struct Ast::Impl {
    Language lang{Language::C};
    TSTree* tree{nullptr};
    std::string source;  // owned bytes the tree references

    Impl() = default;
    ~Impl() {
        if (tree != nullptr) ts_tree_delete(tree);
    }
    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) noexcept        = default;
    Impl& operator=(Impl&&) noexcept = default;
};

} // namespace euxis::parse
