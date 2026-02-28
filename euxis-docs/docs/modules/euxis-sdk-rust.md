# euxis-sdk-rust

`euxis-sdk-rust` contains Rust SDK components for high-performance agent execution and safe systems integration.

## Focus Areas

- Low-latency native execution paths
- Tight memory and CPU envelopes with policy ratcheting
- Cross-platform portability through Rust toolchain compatibility
- Strong correctness and safety foundations for security-sensitive integrations

## Quality Controls

- Build manifest: `euxis-sdk-rust/Cargo.toml`
- Stable package gate: `scripts/quality/run_sdk_rust_tests_stable.sh`
- Package policy: `scripts/perf/package_resource_policy.json`
- Workspace excellence gate: `scripts/quality/validate_package_excellence.py`
