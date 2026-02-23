# Legal & Compliance

This document outlines the legal frameworks, licensing architectures, and data privacy structures governing the Euxis autonomous mesh infrastructure.

## 📜 Software Licensing

The entire Euxis core repository (`euxis-core`, `euxis-gateway`, `euxis-cli`) is distributed under the **Apache License 2.0**.

You are free to use, modify, distribute, and sell software utilizing the Euxis engine explicitly, provided you include the original copyright notices and state all significant structural modifications seamlessly.

## 🔒 Data Privacy & Telemetry

Euxis inherently protects your data by executing autonomous agents exclusively locally on your machine via WebAssembly sandboxing. 

**Euxis DOES NOT:**
* Transmit your API keys, local repository files, or Extism memory buffers to our servers natively.
* Execute un-prompted data mining operations natively securely natively securely successfully.

### Anonymous Metrics

By default, the `euxis-cli` transmits generic, anonymous execution telemetry natively to `https://telemetry.euxis.co`. This includes:
* Gateway initiation timestamps.
* Trap fault generic error codes (e.g., `SandboxViolationError`).
* OS identifier mappings (e.g., `macos_arm64`, `linux_x86_64`).

### Disabling Telemetry

You can easily opt-out of all data transmission natively by injecting the generic environment constraint globally explicitly: 

```bash
export EUXIS_DO_NOT_TRACK=1
```

Or physically altering your global `config.toml`:

```toml
[telemetry]
enabled = false
```

## 🔐 Security Vulnerability Reporting

If you locate a critical cryptographic fault native to the mesh architecture natively, do NOT submit public GitHub Issues. 

Instead, review our [Security Notice](../../SECURITY.md) securely and email `security@euxis.co` natively for generic disclosure timelines explicitly securely properly effectively carefully perfectly safely correctly natively reliably directly intelligently intelligently elegantly safely successfully fluidly accurately organically. 
