# Euxis
**Keep Breathing. We Handle the Rest.**

[![Version][version-badge]][version-url] [![License][license-badge]][license-url]

<br/>

## 1. Context, Not Commands.

Imagine a world where your terminal doesn't wait for you to type. It anticipates.

By 2026, the era of remembering esoteric CLI flags is over. Euxis is built entirely on **Context-Aware Micro-Orchestration**. As you navigate your workspace, a lightweight, quantized background model pre-warms the execution paths.

You don't run commands anymore. You declare *intent*. 
We translate it into action.

<br/>

## 2. The "Apple-Standard" Fluidity.

We believe developer tools should feel as kinetic, responsive, and gorgeous as the apps we build for consumers. Welcome to the **Bento-Grid 2.0**.

### 💧 Liquid Glass Transparency
Whether you're on a KDE Plasma Linux build or macOS Sequoia+, Euxis adapts. Native blurs, ambient shadows, and pixel-perfect typography render directly inside your terminal emulator.

### ⚡ Zero-Jank Architecture
We've forensically eradicated synchronous event-loop blocking by recompiling our hottest code paths (like ANSI stream parsing) into native Rust Native extensions. Your UI thread *never* freezes. 

### 🎭 The "Artifact-Only" Mode
Need absolute focus? Euxis operates in a consumer-first mode, intercepting execution noise and presenting only the generated Markdown or JSON artifacts.

<br/>

## 3. Speed as a Feature. Not a Metric.

We don't just optimize for the happy path. We optimize for the physics of your hardware. Targeting a true 50% reduction in aggregate execution time ($T \le 0.5 T_current$).

- **Unified Binary Translation:** Execution occurs seamlessly across your host OS and containerized sub-agents. 
- **WSL-to-Windows Elimination:** Bypassing I/O latency taxes when crossing the filesystem barrier on Windows Subsystem for Linux using native executable interop.
- **macOS M5 Neural Engine Routing:** Bypassing the CPU bus for local cryptographic logic, communicating natively with Metal Performance Shaders.

<br/>

## 4. The 2026 Engine Room

Euxis is composed of hyper-specialized micro-packages. Use what you need.

| Component | Responsibility | Status |
|---------|-------------|---------|
| [euxis-core](./euxis-core) | The central nervous system | `v0.0.1` |
| [euxis-cli](./euxis-cli) | The orchestrator | `v0.0.1` |
| [euxis-tui](./euxis-tui) | The Liquid Glass interface | `v0.0.1` |
| [euxis-gateway](./euxis-gateway) | The real-time nerve center | `v0.0.1` |
| [euxis-crypto-lib](./euxis-crypto-lib) | The vault | `v0.0.1` |

<br/>

### Immediate Access
```bash
pip install euxis-core euxis-cli euxis-tui

euxis ui
```

<br/>

> *This is not just a tool. It is the new geometry of development.*

— **Designed by Sebastien Rousseau**  
[sebastienrousseau.com](https://sebastienrousseau.com) | [euxis.co](https://euxis.co)

[license-badge]: https://img.shields.io/badge/license-MIT-blue.svg
[license-url]: LICENSE
[version-badge]: https://img.shields.io/badge/version-0.0.1-green.svg
[version-url]: https://github.com/sebastienrousseau/euxis/releases/tag/v0.0.1
