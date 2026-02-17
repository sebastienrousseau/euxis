# TUI (ETX)

The Euxis Terminal Experience built on Textual.

## Purpose
Provides a keyboard-first interface for deploying agents, monitoring sessions, and managing fleet operations.

## Structure
- `app.py` — main Textual app
- `screens/` — UI screens
- `widgets/` — reusable components
- `themes.py` — theme definitions

## Dependencies
- `core/` — shared registry and utilities
- `agents/` — registry and prompts
- `metrics/` — optional performance data

## Usage
```bash
python -m tui
```

## Development
```bash
# Run TUI in dev mode
python -m tui
```

## Testing
```bash
pytest tests/test_tui.py
```

## API / Exports
ETX is launched via `cli/bin/euxis-tui` and uses `tui/app.py` as the entrypoint.
