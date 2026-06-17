/// @file
/// @brief Core types for the euxis Code Property Graph.
///
/// A Code Property Graph (CPG) unifies the AST, the Control Flow
/// Graph (CFG), and the Data Dependency Graph (DDG) under a single
/// node-and-edge model so that callers can write one query that
/// pivots across all three. The classical reference is Yamaguchi
/// et al., "Modeling and Discovering Vulnerabilities with Code
/// Property Graphs" (S&P 2014); Joern is the JVM-side reference
/// implementation.
///
/// This batch lands the AST projection: every named node in a
/// libs/parse Ast becomes a CPG Node, parent-child relationships
/// become `AstChild` edges. CFG (statement successor) and DDG
/// (def-use) land in subsequent batches without changing the public
/// type surface here.
///
/// The CPG is grammar-agnostic at the type level — `NodeKind` is a
/// closed enum chosen to match the union of features the eight
/// libs/parse grammars expose. Grammar-specific tree-sitter node
/// names are translated to NodeKind by the builder.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "euxis/parse/types.hpp"

namespace euxis::cpg {

/// Stable handle for a node in a Graph. `0` is the reserved "null"
/// id; the root node lives at id `1`. Ids are dense in [1, N] so a
/// vector keyed on id is a valid storage choice.
struct NodeId {
    std::uint32_t value{0};

    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return value != 0;
    }
    [[nodiscard]] friend constexpr auto operator==(NodeId, NodeId) noexcept
        -> bool = default;
};

/// Sentinel for "no node".
inline constexpr NodeId kNullNode{0};

/// Closed taxonomy of node kinds the CPG models. Anything a grammar
/// emits that we haven't mapped lands as `Unknown`; the original
/// tree-sitter kind name is preserved on the Node so consumers can
/// still pattern-match on it.
enum class NodeKind : std::uint8_t {
    Unknown,
    TranslationUnit,    ///< Root node — one per parsed file.
    FunctionDef,        ///< Function/method definition.
    Parameter,          ///< Formal parameter of a FunctionDef.
    Block,              ///< Compound statement.
    Call,               ///< Function/method invocation.
    Identifier,         ///< Variable / function name reference.
    Literal,            ///< Numeric / string / boolean literal.
    Assignment,         ///< Assignment expression / statement.
    Return,             ///< Return statement.
    If,                 ///< If statement.
    Loop,               ///< for / while / do.
    Declaration,        ///< Variable / type declaration.
    PreprocInclude,     ///< C/C++ #include, Rust use, Go import, …
    Comment,            ///< Single- / multi-line comment.
};

[[nodiscard]] auto node_kind_str(NodeKind k) noexcept -> const char*;

/// Edge kinds the CPG supports. Future batches add Cfg / Ddg / Call
/// without touching the public type. `AstChild` is the only kind
/// the Week-8a builder emits.
enum class EdgeKind : std::uint8_t {
    AstChild,           ///< Parent -> child in the syntax tree.
    Cfg,                ///< Predecessor -> successor in control flow (P1.3b).
    Ddg,                ///< Definition -> use in data flow (P1.3c).
    Call,               ///< Caller -> callee (P1.5 once name resolution lands).
};

[[nodiscard]] auto edge_kind_str(EdgeKind k) noexcept -> const char*;

/// A single CPG node.
struct Node {
    NodeId id{kNullNode};
    NodeKind kind{NodeKind::Unknown};

    /// Original tree-sitter node kind name (e.g. "function_definition").
    /// Useful for debugging and for the long-tail of grammar nodes the
    /// closed `NodeKind` taxonomy doesn't model directly.
    std::string raw_kind;

    /// Identifier name for nodes that carry one (FunctionDef name,
    /// Identifier reference, etc.). Empty when not applicable.
    std::string name;

    /// Source range copied from the libs/parse Ast.
    euxis::parse::SourceRange range{};

    /// Language the node was parsed under. Surfaced here so cross-
    /// language CPGs (a project with both C++ and Python) keep node
    /// origin visible without a separate map.
    euxis::parse::Language language{euxis::parse::Language::C};
};

/// A directed edge.
struct Edge {
    NodeId source{kNullNode};
    NodeId target{kNullNode};
    EdgeKind kind{EdgeKind::AstChild};
};

/// Classify a tree-sitter node-type string into a CPG NodeKind for
/// the given language. Grammar-specific naming differences are
/// handled here so the builder can stay grammar-agnostic.
[[nodiscard]] auto classify(euxis::parse::Language lang,
                            std::string_view raw_kind) noexcept -> NodeKind;

} // namespace euxis::cpg
