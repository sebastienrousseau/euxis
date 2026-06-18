<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::cpg</h1>

<p align="center">
  Code Property Graph for euxis: builds an AST projection, an
  intra-procedural CFG, and a forward DDG into one navigable
  <code>Graph</code> of typed nodes and edges.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Public surface](#public-surface)
- [Graph model](#graph-model)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/cpg)
target_link_libraries(my_app PRIVATE euxis-cpg-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/cpg/builder.hpp"
#include "euxis/cpg/cfg.hpp"
#include "euxis/cpg/ddg.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/parse/types.hpp"

int main() {
    using namespace euxis::cpg;

    euxis::parse::Parser p{euxis::parse::Language::Cpp};
    auto ast = p.parse(read_file("src/example.cpp"));

    auto graph = build_from_ast(*ast);                  // AST projection
    build_cfg(graph, CfgBuildOptions{});                // intra-procedural CFG
    build_ddg(graph, DdgBuildOptions{});                // forward data-dependency graph

    std::cout << "nodes: " << graph.node_count()
              << " edges: " << graph.edge_count() << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `types.hpp` | `NodeId`, `NodeKind`, `EdgeKind`, `Node`, `Edge` |
| `graph.hpp` | `Graph` — owns the node/edge storage with neighbour iteration |
| `builder.hpp` | `build_from_ast(ast) -> Graph`; `BuildError` |
| `cfg.hpp` | `build_cfg(graph, opts)`; `CfgBuildOptions`, `CfgStats` |
| `ddg.hpp` | `build_ddg(graph, opts)`; `DdgBuildOptions`, `DdgStats` |

## Graph model

Nodes carry a `NodeKind` (function, parameter, call, identifier, return, ...) and an optional `SourceRange` from the originating tree-sitter parse. Edges carry an `EdgeKind`:

- `Ast` — original parent/child relationship from the AST projection
- `CfgSuccessor` — control-flow successor inside one function body
- `DataDependency` — forward DDG edge from definition to use

The graph stays intra-procedural by design. Inter-procedural reasoning (callgraph, reachability) is layered on top by `libs/reach`.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
