# euxis-etx

Qt6 desktop GUI for the Euxis platform — fleet management, agent deployment, adaptive themes.

## Overview

euxis-etx is the Euxis desktop application built with Qt6. It provides 9 screens (welcome, dashboard, agent, about, fleet monitor, settings, help, logs, playbooks), 6 reusable widgets (header with sparkline, shortcut bar, fleet grid, agent card, search bar, tip bar), and 3 QSS themes (liquid-glass, calm, focused) with adaptive theme logic that adjusts based on ambient conditions. Navigation supports vim-style keybindings for efficient keyboard-driven operation.

## Dependencies

- Qt6 (Widgets, Network)
- All euxis-cpp libraries (euxis-crypto-cpp, euxis-bridge-cpp, euxis-memory-cpp, euxis-identity-cpp, euxis-inference-cpp, euxis-a2a-cpp)

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-etx
```

## Testing

```bash
ctest --test-dir build -R euxis-etx_tests
```

## API

- **app.hpp** -- EuxisApp: main application class, screen lifecycle, and navigation stack.
- **theme.hpp** -- ThemeManager: QSS theme loading (liquid-glass, calm, focused) and adaptive switching.
- **config.hpp** -- AppConfig: persistent user settings, keybinding configuration, window geometry.
- **registry.hpp** -- WidgetRegistry: central registry for screen and widget instantiation.
