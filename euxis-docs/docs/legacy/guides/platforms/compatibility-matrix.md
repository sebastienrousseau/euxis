# Cross-Platform Compatibility Matrix

Euxis builds and executes deterministically across all major infrastructure operating systems.

| Core Capability | Linux (Native) | macOS (Apple Silicon) | macOS (Intel) | Windows (WSL2) |
| --- | :---: | :---: | :---: | :---: |
| **Gateway Orchestration** | ✅ Native | ✅ Native | ✅ Native | ✅ Native |
| **WebAssembly Sandboxing** | ✅ Native | ✅ Native | ✅ Native | ✅ Linux Boundary |
| **CLI / TUI Interface** | ✅ Native | ✅ Native | ✅ Native | ✅ Native |
| **Inter-Agent Mesh**| ✅ Flawless | ✅ Flawless | ✅ Flawless | ⚠️ Limited (IO bounds) |
| **Cryptographic Signatures** | ✅ Hardware | ✅ Secure Enclave | ✅ Generic | ✅ Generic |
| **Systemd Integration** | ✅ Native | ❌ User Daemons | ❌ User Daemons | ⚠️ Requires `wsl.conf` |

## Support Boundaries

The Euxis Core Team prioritizes support architectures in the following order:
1. Native Linux (Ubuntu, Debian, Arch)
2. macOS (Apple Silicon)
3. Windows (WSL2 via Windows 11)
4. macOS (Intel)
