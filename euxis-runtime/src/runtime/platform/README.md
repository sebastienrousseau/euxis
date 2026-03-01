# runtime.platform

This directory isolates platform-specific behavior.

Targets:
- macOS
- Linux
- WSL

Guidelines:
- expose platform behavior through contracts in `contracts.py`
- keep core logic under `../core` platform-agnostic
- avoid importing concrete adapters directly from core modules
