<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::parse</h1>

<p align="center">
  Tree-sitter parser front-end for euxis. One <code>Parser</code> type
  produces a uniform <code>Ast</code> across eight languages:
  C, C++, Rust, Go, Python, JavaScript, TypeScript, Java.
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
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/parse)
target_link_libraries(my_app PRIVATE euxis-parse-cpp)
```

The grammar libraries (`tree-sitter-c`, `tree-sitter-cpp`, `tree-sitter-rust`, etc.) are fetched at configure time. No extra system setup is needed.

## Quick start

```cpp
#include <iostream>
#include <string>

#include "euxis/parse/parser.hpp"
#include "euxis/parse/types.hpp"

int main() {
    using namespace euxis::parse;

    Parser parser{Language::Cpp};
    const std::string code = R"(int main() { return 0; })";

    auto ast = parser.parse(code);
    if (!ast) {
        std::cerr << "parse error: " << ast.error().message << '\n';
        return 1;
    }

    std::cout << "root: " << ast->root().kind()
              << " (children: " << ast->root().child_count() << ")\n";
}
```

## Public surface

| Header | What it owns |
|---|---|
| `types.hpp` | `Language` enum (C, Cpp, Rust, Go, Python, JavaScript, TypeScript, Java), `SourceRange`, `ParseError` |
| `parser.hpp` | `Parser` — wraps a tree-sitter parser instance for one language |
| `ast.hpp` | `Ast` — owns the parse tree; node accessors uniform across languages |

## Examples

### Walk every function definition in a Rust file

```cpp
#include "euxis/parse/parser.hpp"

Parser p{Language::Rust};
auto ast = p.parse(read_file("src/main.rs"));
ast->walk([](const auto& node) {
    if (node.kind() == "function_item") {
        std::cout << node.range().start_line << ": " << node.text() << '\n';
    }
});
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
