# Euxis C++ SDK Examples

First-party examples for embedding the Euxis SDK (`libs/`) in your own
C++23 applications. Each example is a self-contained CMake target that
links against the same static libraries shipped with the `euxis` binary.

## Examples

| Example | What it shows |
|---------|---------------|
| [`a2a_minimal_server/`](a2a_minimal_server/) | Minimal A2A v0.2 server handler — card, validation, full task lifecycle |
| [`agent_loop_demo/`](agent_loop_demo/) | `AgentLoopHarness` + `CredentialPool` + per-session insights wired end-to-end |
| [`tool_calling_loop/`](tool_calling_loop/) | Closed tool-calling loop with `classify_approval` gating and `IToolRegistry` dispatch |

## Building

Examples are opt-in. Enable with `-DEUXIS_BUILD_EXAMPLES=ON` at configure
time:

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build
```

Built binaries land under
`cmake-build/docs/examples/cpp/<example>/euxis_example_<name>`.

## Adding a new example

1. Create `docs/examples/cpp/<name>/` with `CMakeLists.txt`, `main.cpp`,
   and a `README.md`.
2. Add `add_subdirectory(<name>)` to `docs/examples/cpp/CMakeLists.txt`.
3. Match the existing example's CMake style: link only the libs your
   example actually uses; declare them with `PRIVATE` visibility.
4. The binary target name should be `euxis_example_<name>` to keep ctest
   and `cmake --build --target` discoverable.

## Why not in `apps/`?

`apps/` ships production binaries (`euxis-cli`, `euxis-gateway`,
`euxis-etx`, `euxis-publisher`). Examples are educational artefacts —
they live alongside the documentation they support and are built only
on demand.
