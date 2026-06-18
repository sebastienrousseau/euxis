<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::publisher</h1>

<p align="center">
  Document rendering pipeline for euxis. Renders LaTeX, Markdown, and JSON
  from structured YAML data using <code>inja</code> templates with
  LaTeX-safe delimiters.
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
- [Template delimiters](#template-delimiters)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/publisher)
target_link_libraries(my_app PRIVATE euxis-publisher-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/publisher/publisher.hpp"

int main() {
    using namespace euxis::publisher;

    Publisher pub{
        Publisher::Config{
            .content_root = "./content",                 // holds data/meta.yaml + templates/
            .build_mode   = BuildMode::Draft,
        },
    };

    auto rendered = pub.render("cv-doc", OutputFormat::LaTeX);
    if (!rendered) {
        std::cerr << "render failed: " << rendered.error() << '\n';
        return 1;
    }
    std::cout << "rendered " << rendered->size() << " bytes of LaTeX\n";
}
```

## Public surface

| Header | What it owns |
|---|---|
| `publisher.hpp` | `Publisher` (`render`, `build_pdf`), `OutputFormat` (LaTeX, Markdown, JSON), `BuildMode` (Draft, Submission, CameraReady), `DataNode` concept |

`Publisher::render` returns `std::expected<std::string, std::string>` so callers chain `.and_then` / `.transform` instead of catching exceptions.

## Template delimiters

LaTeX clashes with the default `{{` / `{%` Jinja delimiters. The publisher configures `inja` with LaTeX-safe sequences:

| Role | Delimiter |
|---|---|
| Expression | `<< variable >>` |
| Statement  | `<% if cond %> … <% endif %>` |
| Comment    | `<# author note #>` |

A template stays compilable by `latexmk` even before substitution.

## Examples

### Chain a render through a syntax-validation step

```cpp
auto result = publisher.render("cv-doc", OutputFormat::LaTeX)
    .and_then([](std::string&& tex) { return validate_latex(tex); })
    .transform([](std::string&& valid_tex) { return compress(valid_tex); });
```

### Build a PDF

```cpp
auto pdf_path = publisher.build_pdf("cv-doc");
```

`build_pdf` requires `latexmk` on `PATH`. The output PDF is materialised in `content_root/build/<name>.pdf`.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
