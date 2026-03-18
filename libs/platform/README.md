# Euxis Platform C++

The `euxis::platform` module is the strict Platform Abstraction Layer (PAL) for the Euxis Agent OS. It encapsulates all hardware-specific APIs, compiler intrinsics, and OS-level security constraints (macOS, Linux, WSL).

## Architectural Boundaries

This module is strictly internal. Applications (`apps/`) and public libraries (`libs/`) must never instantiate platform types directly. 

* **Precondition**: Host OS meets minimal kernel requirements (e.g., Linux 5.15+ with eBPF).
* **Postcondition**: Exposes a unified `euxis::runtime::Platform` interface.

To ensure "Outcome Perfect" stability, `#ifdef` macros are strictly quarantined within this module. 

## OS-Level Hardening

The Euxis Agent OS relies on hardware-backed execution boundaries rather than fragile application-level checks.

* **eBPF**: Extended Berkeley Packet Filter — strict kernel-level syscall supervision.
* **KVM**: Kernel Virtual Machine — hardware-enforced microVM execution.

On Linux, invoke `enforce_micro_kernel_sandbox()` to load the eBPF Seccomp profiles. This restricts the Extism WASM runtime to only read, write, and mmap operations, guaranteeing memory safety against rogue agents.

## Cross-Platform Parity

When targeting WSL (Windows Subsystem for Linux), the platform layer autonomously bridges the execution gap.

* **Interop**: Invoke `detect_type()` to discover WSL boundaries.

For seamless cross-platform functionality, the PAL maps clipboard operations and URL invocations to their native Windows equivalents (e.g., `clip.exe` and `wslview`), preventing "interop-leakage" while maintaining elevated performance.