<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">apps/publisher — <code>euxis-publisher</code></h1>

<p align="center">
  Release-packager binary. Wraps <code>libs/publisher</code> in a one-shot
  CLI for building signed PDF / LaTeX / Markdown deliverables from a
  content tree.
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
- [Inputs](#inputs)
- [Outputs](#outputs)
- [License](#license)

---

## Install

```bash
cmake -B build/cmake-build
cmake --build build/cmake-build --target euxis-publisher
```

## Quick start

```bash
euxis-publisher \
    --content-root ./content \
    --document cv-doc \
    --format pdf \
    --mode submission
```

## Inputs

The content root holds two subtrees:

```
content/
├── data/
│   └── meta.yaml          # structured data (cv, project list, ...)
└── templates/
    ├── cv-doc.tex.tmpl    # inja template, LaTeX-safe delimiters
    └── …
```

YAML data is converted to `nlohmann::json` via the `DataNode` concept (see `libs/publisher`) and fed to the template.

## Outputs

| Format | Notes |
|---|---|
| `latex` | Raw `.tex` written to `content/build/<doc>.tex` |
| `pdf`   | Calls `latexmk` (must be on `PATH`); output at `content/build/<doc>.pdf` |
| `markdown` | Renders the same template tree as plain Markdown |
| `json`  | Dumps the merged data context — useful for debugging template variable resolution |

Signed evidence bundles (DSSE + in-toto + SLSA) for each output are produced by `libs/attest` when `--sign` is passed.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
