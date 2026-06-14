/// @file
/// @brief Built-in source / sink / sanitizer library.
///
/// The default spec covers the highest-frequency taint patterns
/// across the 8 wired languages. It is intentionally a starting
/// floor, not an exhaustive corpus — the real spec library is
/// imported from OpenGrep's taint rule pack and the IRIS / AdaTaint
/// mined corpora in subsequent batches.
///
/// Design principle: every spec entry is a *named* concept (env
/// access, system call, eval, SQL execute, HTML write, …) rather
/// than a string literal. Adding a new language reuses the same
/// concept names — keeps the cross-language story consistent.
#pragma once

#include "euxis/taint/spec.hpp"

namespace euxis::taint {

/// Return the default spec covering common sources / sinks /
/// sanitizers across C, C++23, Rust, Go, Python, JavaScript,
/// TypeScript, and Java.
[[nodiscard]] auto builtin_specs() -> TaintSpec;

} // namespace euxis::taint
