# Installation Guide

Install Euxis permanently into your environment. We distribute the core framework via standard Python package indexes and our ultra-fast Rust dependencies natively compiled for your platform.

## Pre-Requisites

Before installing Euxis, you must possess the following OS-level integrations:
* **Python Environment:** Version 3.10, 3.11, or 3.12 exclusively. We do not support Python 3.9 downwards.
* **WASI Toolchain:** You need the `wasm32-wasi` standard library available on your machine to execute native agents.

### Checking for WASI

Check if `wasm32-wasi` is installed on your Linux or macOS system:

```bash
rustup target add wasm32-wasi
```

## Global Installation (Recommended)

To orchestrate agents system-wide, use the `uv` package manager or `pipX` to isolate the environment cleanly without polluting your host `pip` binaries.

### Using `uv` (Fastest)

1. Ensure [uv](https://docs.astral.sh/uv/) is installed on your host system.
2. Install the Euxis core toolchain in milliseconds:

```bash
uv tool install euxis-cli 
uv tool install euxis-core 
uv tool install euxis-tui
```

### Using Standard `pip` (Development)

If you intend to contribute directly to the agent framework, pull the source using Git and install locally inside a virtual environment.

```bash
git clone https://github.com/sebastienrousseau/euxis.git
cd euxis
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
pip install -e ./euxis-core -e ./euxis-cli -e ./euxis-tui
```

## Verifying the Installation 

After installation is complete, verify the CLI executable is correctly exposed in your `$PATH`.

```bash
euxis --version
```

If the system returns `Euxis CLI v0.0.2 (Gateway Operational)`, the installation succeeded. Proceed to the [Quick Start Guide](quick-start.md).
