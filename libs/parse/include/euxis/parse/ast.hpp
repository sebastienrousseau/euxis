/// @file
/// @brief Abstract syntax tree handle.
///
/// `Ast` owns the parsed tree and the source bytes the tree refers
/// to. Move-only — the underlying tree-sitter handle is heavyweight
/// and we explicitly do not copy it. PIMPL'd so consumers don't pull
/// in `<tree_sitter/api.h>` transitively.
///
/// The visitor surface is deliberately narrow at this stage: walk
/// every named node in document order with a callback. Queries
/// (tree-sitter's S-expression query language) and incremental
/// reparse hooks land in P1.3 / P1.7 once the CPG and reachability
/// pruner need them.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "euxis/parse/types.hpp"

namespace euxis::parse {

class Ast {
public:
    Ast() noexcept;
    ~Ast();

    Ast(const Ast&)            = delete;
    Ast& operator=(const Ast&) = delete;
    Ast(Ast&&) noexcept;
    Ast& operator=(Ast&&) noexcept;

    /// Language the tree was parsed under. Invariant after parse.
    [[nodiscard]] auto language() const noexcept -> Language;

    /// Bytes that were parsed. The Ast owns the storage; references
    /// returned from `node_text()` point into this buffer and stay
    /// valid for the lifetime of the Ast.
    [[nodiscard]] auto source() const noexcept -> std::string_view;

    /// Tree-sitter reports `has_error` when at least one ERROR node
    /// was inserted for recovery. A `true` here does NOT mean the
    /// AST is unusable — tree-sitter recovers and produces a partial
    /// tree, which is often good enough for pattern matching. It DOES
    /// mean the source had at least one syntax error.
    [[nodiscard]] auto has_errors() const noexcept -> bool;

    /// Source range of the root node (always 0 → end_byte for a
    /// well-formed parse).
    [[nodiscard]] auto root_range() const noexcept -> SourceRange;

    /// Kind name of the root node (e.g. "translation_unit" for C).
    [[nodiscard]] auto root_kind() const noexcept -> std::string_view;

    /// Number of named children of the root.
    [[nodiscard]] auto root_named_child_count() const noexcept -> std::uint32_t;

    /// Visit every named node in pre-order. Anonymous nodes
    /// (punctuation, keywords) are skipped. The visitor receives the
    /// node's kind name and its source range; if it returns `false`,
    /// descent into that node's children is suppressed.
    using Visitor = std::function<bool(std::string_view kind, SourceRange)>;
    void visit_named_nodes(const Visitor& visit) const;

    /// Count of named nodes in the tree (including the root).
    [[nodiscard]] auto count_named_nodes() const noexcept -> std::size_t;

    /// Source bytes covered by `range`.
    [[nodiscard]] auto node_text(SourceRange range) const noexcept
        -> std::string_view;

private:
    friend class Parser;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace euxis::parse
