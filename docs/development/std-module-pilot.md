# `import std;` C++23 modules pilot

The euxis build optionally supports the C++23 standard library module (`import std;`) on toolchains that ship a precompiled `std.cppm`. Today that means Homebrew LLVM 19+; Apple Clang's libc++ still lacks the module source file, so `import std;` is not available on the macOS system toolchain even though Apple Clang 21 advertises C++23 support.

## What this enables

Replacing `#include <vector>` + `#include <string>` + `#include <expected>` (etc.) with a single `import std;` per translation unit. Faster compile times once the std module BMI is built, and a single declaration imports the whole standard library without preprocessor exposure to header internals.

## How to opt in (per-library or per-target)

The `cmake/EuxisStdModule.cmake` helper is wired into the root `CMakeLists.txt` but **disabled by default**. The default `make cpp-configure` does *not* try to precompile `std.cppm` — that keeps Apple Clang builds green, because Apple's libc++ ships a `std.cppm` whose precompile path our flag set doesn't accept cleanly.

To opt in, configure with `-DEUXIS_ENABLE_STD_MODULE=ON`:

```sh
cmake -B build/cmake-build -S . -DEUXIS_ENABLE_STD_MODULE=ON
```

When the flag is on, the helper detects whether the compiler can actually precompile `std.cppm` and emits an INTERFACE target `euxis::std_module` carrying `-stdlib=libc++` + `-fmodule-file=std=<bmi-path>`.

```cmake
# In your library or target:
if(TARGET euxis::std_module)
    target_link_libraries(my_target PRIVATE euxis::std_module)
    target_compile_options(my_target PRIVATE -fmodules)
    target_compile_definitions(my_target PRIVATE EUXIS_USE_STD_MODULE=1)
endif()
```

In source files behind the flag:

```cpp
#if defined(EUXIS_USE_STD_MODULE)
import std;
#else
#include <expected>
#include <string>
#include <vector>
// ...
#endif
```

## What the helper does

1. Probes `CMAKE_CXX_COMPILER_ID` for Clang or AppleClang.
2. Refuses to enable on Clang < 19 (the first LLVM release with `std.cppm` in the libc++ tree).
3. Looks for `<llvm_prefix>/share/libc++/v1/std.cppm` — the canonical install path. Apple Clang does not ship this file; that's the failure mode the helper guards against.
4. When all checks pass, adds a custom-command that precompiles `std.cppm` into a Binary Module Interface (`.pcm`) at configure time.
5. Exposes the BMI via an INTERFACE target carrying `-stdlib=libc++` and `-fmodule-file=std=<path>` so consumers don't have to remember the flags.

## Current toolchain status

| Toolchain | `import std;` | Notes |
|---|---|---|
| Homebrew LLVM 22.1.7 | ✅ Works | Pilot validated on `2026-06-16`; precompile + link succeeds, runtime output correct. |
| Apple Clang 21 | ❌ Not available | libc++ install does not ship `share/libc++/v1/std.cppm`. |
| GCC 14 | ⚠ Partial | libstdc++ has C++23 module support but no precompiled std module ships; build-time module discovery still being stabilised. Helper currently no-ops on GCC. |

## When to migrate a library

Pilot one small library first (suggested: `libs/cache` — single header, low coupling) so any unexpected interaction surfaces in isolation. Wider migration follows when:

- Build-time benchmarks confirm the speedup is worth the build-system complexity.
- Apple Clang ships `std.cppm` (no release date announced as of 2026-06).
- libstdc++ closes its module bring-up story so GCC users have parity.

Until then, ship `import std;` behind the `EUXIS_USE_STD_MODULE` flag in libraries where the win is clearest and let the toolchain detection decide whether each developer's build gets the win.

## Recovery path

If the BMI gets stale (libc++ version bump, compiler upgrade) the symptom is link or compile errors quoting `std.pcm`. Recovery is a clean reconfigure:

```sh
rm -rf build/cmake-build/euxis_std.pcm
cmake -B build/cmake-build -S .
```

The custom command rebuilds the BMI at next make.

## See also

- [`cmake/EuxisStdModule.cmake`](../../cmake/EuxisStdModule.cmake) — detection + enablement.
- LLVM documentation on building the std module: <https://libcxx.llvm.org/Modules.html>.
- C++23 module proposal: [P2465R3](https://wg21.link/P2465R3) — `std` and `std.compat` module names.
