<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::scan</h1>

<p align="center">
  OpenGrep-compatible rule engine for euxis. Loads YAML rule packs,
  runs <code>pattern</code> / <code>pattern-not</code> / <code>pattern-inside</code>
  queries against tree-sitter ASTs, emits <code>euxis::security::Finding</code>s.
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
- [Rule shape](#rule-shape)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/scan)
target_link_libraries(my_app PRIVATE euxis-scan-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/scan/rule_loader.hpp"
#include "euxis/scan/rule_engine.hpp"
#include "euxis/parse/parser.hpp"
#include "euxis/parse/types.hpp"

int main() {
    using namespace euxis::scan;

    auto pack = load_rule_pack_file("./rules/cpp-xss.yaml");
    if (!pack) { std::cerr << "load failed\n"; return 1; }

    euxis::parse::Parser parser{euxis::parse::Language::Cpp};
    auto ast = parser.parse(read_file("src/handlers/echo.cpp"));

    const auto result = run_pack(*pack, *ast, /*path=*/"src/handlers/echo.cpp");
    std::cout << "findings: " << result.findings.size() << '\n';
}
```

## Public surface

| Header | What it owns |
|---|---|
| `rule.hpp` | `RuleSeverity`, `PatternKind` (`Pattern`, `PatternNot`, `PatternInside`), `Pattern`, `RuleMetadata`, `Rule`, `RulePack` |
| `rule_loader.hpp` | `load_rule_pack` (string), `load_rule_pack_file` (path); `LoadError` |
| `rule_engine.hpp` | `run_pack(rules, ast, path) -> EngineResult` + `EngineStats` |

## Rule shape

```yaml
rules:
  - id: cpp/xss-echo-untrusted
    severity: high
    pattern: 'echo($INPUT)'
    pattern-not: 'echo(escape($INPUT))'
    pattern-inside: 'class Handler { ... }'
    message: 'XSS sink reaches a Handler method without escape()'
```

`pattern-not` excludes safe call sites; `pattern-inside` scopes the match to a containing AST shape. Compatible with the OpenGrep `v2` rule format.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
