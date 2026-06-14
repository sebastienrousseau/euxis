/// @file
/// @brief Project a libs/parse Ast into a CPG Graph.
///
/// Walks every named node in the parsed Ast, classifies it via
/// `cpg::classify()`, and adds an AstChild edge from the current
/// parent to the new node. CFG (statement successor) and DDG
/// (def-use) construction layer on top of this projection in
/// subsequent batches without changing the surface.
#pragma once

#include <expected>
#include <string>

#include "euxis/cpg/graph.hpp"
#include "euxis/parse/ast.hpp"

namespace euxis::cpg {

struct BuildError {
    std::string message;
};

/// Build a CPG Graph from a parsed Ast. The Builder consumes the
/// Ast by reference — the Ast must outlive the returned Graph
/// because Node::name views and SourceRange snippets are stored
/// owned (string copies) so the Graph itself is self-sufficient
/// for serialisation, but callers that re-query node_text via the
/// Ast obviously need the Ast still around.
[[nodiscard]] auto build(const euxis::parse::Ast& ast)
    -> std::expected<Graph, BuildError>;

} // namespace euxis::cpg
