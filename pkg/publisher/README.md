# Euxis Publisher: C++23 High-Performance Rendering

The Euxis Publisher module implements a hardware-native document generation pipeline. It replaces legacy Python bottlenecks with C++23 velocity. Use this module to render LaTeX, Markdown, or JSON from structured YAML data.

## Core Architectural Requirements

For low-latency execution, the publisher utilizes `inja` for templating and `yaml-cpp` for data parsing. Access these capabilities via the `euxis::publisher::Publisher` class. 

*   **Precondition**: Ensure the `content_root` contains valid `data/meta.yaml` and `templates/` directories.
*   **Postcondition**: Return a `std::expected` containing the rendered artifact or a descriptive error string.

## Template Engine & Delimiters

LaTeX templates often conflict with default Jinja2 delimiters like `{{` and `{%`. The Euxis Publisher enforces custom sequences to ensure **zero escape character collisions**.

*   **Expression**: `<< variable >>`
*   **Statement**: `<% block %>`
*   **Comment**: `<# note #>`

```cpp
#include <inja/inja.hpp>

// Configure environment with LaTeX-safe delimiters
auto configure_env() -> inja::Environment {
    inja::Environment env;
    env.set_expression("<<", ">>"); // variable equivalent
    env.set_statement("<%", "%>");   // block equivalent
    env.set_comment("<#", "#>");
    return env;
}
```

## Monadic Error Handling

Eliminate legacy `try-catch` blocks in your hot paths. Utilize C++23 monadic operations to chain rendering steps. This pattern ensures **memory safety** by preventing access to uninitialized results.

*   **RAII**: Resource-bound lifetime management.
*   **std::expected**: Monadic error container — Success or failure.

```cpp
auto result = publisher.render("cv-doc")
    .and_then([](std::string&& latex) {
        return validate_syntax(latex); // Ensure LaTeX integrity
    })
    .transform([](std::string&& valid_latex) {
        return compress_payload(valid_latex); // Post-process result
    });
```

## Memory Residency & Zero-Copy I/O

The publisher aligns with the **ZeroClaw <5MB footprint benchmark**. It avoids redundant heap allocations by mapping binary snapshots directly to memory via `mmap`.

*   **std::string_view**: Non-owning string reference — Zero-allocation string views.
*   **std::span**: Bounds-checked memory view — Safe contiguous access.

> **Warning: Never return a `std::string_view` to a local temporary string. Doing so triggers Use-After-Free Undefined Behavior (UB).**

## SIMD-Aware Data Translation

Data ingestion utilizes a `yaml_to_json` recursive visitor. For large-scale batch publishing, organize your YAML sequences to exploit **Structure-of-Arrays (SoA)** cache locality.

*   **SoA**: Hardware-friendly data layout — Cache-optimized memory alignment.
*   **Concepts**: Compile-time template constraints.

```cpp
/**
 * @brief Constraint for valid YAML/JSON data nodes.
 */
template <typename T>
concept DataNode = requires(T t) {
    { t.IsMap() } -> std::convertible_to<bool>;
    { t.IsSequence() } -> std::convertible_to<bool>;
};

// Map YAML nodes to JSON for Inja ingestion
auto load_safe(const DataNode auto& node) -> nlohmann::json {
    return yaml_to_json(node); // Execute recursive conversion
}
```

## Binary Artifact Generation

The `build_pdf` method orchestrates the transition from LaTeX to PDF. It utilizes memory-mapped `.msgp` files to maintain **state-aware idempotency**.

*   **Type Erasure**: Hiding concrete implementations — Abstract interface dispatch.

**Precondition**: A valid LaTeX installation (`latexmk`) must be present in the system `PATH`. 

**All core logic refactors must be secured via signed commits to maintain the performance baseline integrity.**
