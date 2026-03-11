# Euxis Scripts C++

The `scripts-cpp` module provides the build-time and execution-time utilities for the Euxis Agent OS. It implements compiled C++23 tooling to replace slow bash scripts in the CI/CD pipeline.

## Build-Time Optimization

The tooling is compiled native logic, integrated directly into the CMake build graph.

* **AOT**: Ahead-Of-Time compilation — Use pre-compiled binaries for speed.
* **Postcondition**: Exits with code `0` on success, `>0` on validation failure.

Execute these utilities to guarantee 100% Quality Gate coverage before artifact release. The tools bypass interpretive overhead, running structural and schema assertions in sub-10ms.

## Codebase Governance

Ensure code conforms to the "Premium Tech" standard.

```cpp
auto res = validate_module(ast_tree)
    .and_then([]() { return generate_report(); })
    .or_else([](auto&& err) { return halt_build(err); });
```

Utilize C++23 monadic operations to halt CI execution immediately upon detecting legacy headers, `std::endl`, or lack of `[[likely]]` / `[[unlikely]]` hints in critical hot paths.