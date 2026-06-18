<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">apps/etx — <code>euxis-etx</code></h1>

<p align="center">
  Qt6 desktop UI for euxis. Chat surface, fleet registry browser, theme
  + accessibility engine, OAuth provider sign-in flows, and a config editor.
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
- [Source layout](#source-layout)
- [Theming + accessibility](#theming--accessibility)
- [License](#license)

---

## Install

ETX requires Qt6 (Widgets, Quick, Network, LinguistTools). Provision it via your distro's Qt6 SDK, Homebrew (`brew install qt`), or the Qt online installer.

```bash
cmake -B build/cmake-build
cmake --build build/cmake-build --target euxis-etx
./build/cmake-build/apps/etx/euxis-etx
```

## Quick start

Launch `euxis-etx`, choose a provider on the welcome screen, sign in via OAuth (or paste an API key into the secrets keychain), and the dashboard wires up. Chat messages are dispatched against `libs/inference`; the fleet registry pane loads `data/agents/registry.json` via `libs/cache`.

## Source layout

| File | What it owns |
|---|---|
| `main.cpp` | Qt application entry, locale + translations bootstrap |
| `app.cpp`  | Top-level `QApplication` + window orchestrator |
| `chat_engine.cpp` | Dispatches user input against `libs/inference` (sync + streaming) |
| `ghost_text.cpp` | In-line autocomplete suggestions |
| `registry.cpp` | Loads + caches the agent registry |
| `oauth_flow.cpp` | Provider OAuth dance (`QNetworkAccessManager`-backed) |
| `config.cpp` | Reads / writes `~/.config/euxis/etx.toml` |
| `theme.cpp`, `semantic_colors.cpp` | Light / dark / contrast themes |
| `accessibility.cpp` | Screen-reader hints, focus-trap fallbacks |
| `screens/` | One file per top-level screen (welcome, dashboard, chat, settings, ...) |
| `widgets/` | Reusable QWidgets: chat bubble, code-block viewer, status pill |

## Theming + accessibility

`theme.cpp` exposes three named palettes (light, dark, high-contrast). `accessibility.cpp` mirrors the same palette tokens onto Qt's accessibility API so screen readers describe interactive elements correctly. Translations live under `apps/etx/translations/` and are wired in via `qt6_add_translations`.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
