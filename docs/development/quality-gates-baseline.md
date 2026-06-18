# Quality-gate baseline (2026-06-18)

Snapshot of the three reconnaissance gates from issue
[#42](https://github.com/sebastienrousseau/euxis/issues/42) measured
locally on `feat/v0.0.3` at `chore/issue-42-quality-gates`. Run with
LLVM 22.1.7, AppleClang 21.0.0, gcovr 8.6, googletest pinned by
CMake fetch.

## Gate 1 — sanitizer test pass (ASan + UBSan)

**Status:** **green** (was 7/18 in the May reconnaissance).

```
ctest --test-dir cmake-build-san --output-on-failure -j
100% tests passed, 0 tests failed out of 31
Total Test time (real) =  97.23 sec
```

Zero ASan reports, zero UBSan reports across 1 394 gtest cases inside
the CLI binary alone (plus 30 other test targets).

### Real bugs caught + fixed in this sweep

- `apps/cli/tests/test_registry_client.cpp` — `RegistryClientCacheTest`
  fixture stored a `const char*` from `std::getenv("XDG_CACHE_HOME")` and
  used it after a subsequent `::setenv()`, which can `realloc()` the
  environ block and free the original storage. Heap-use-after-free on
  TearDown. Now snapshots to `std::optional<std::string>`.
- `apps/cli/tests/test_cmd_infra.cpp` — `InfraCmdTest.DaemonStartWithStalePid`
  forked a real daemon via `cmd_daemon("start")` whose child kept writing
  files inside `ctx.euxis_home` while the fixture's `TearDown` called
  `fs::remove_all` on the same path. ENOTEMPTY race. Now polls for the
  child's PID and calls `cmd_daemon("stop")` before TearDown.
- `libs/attest/src/dsse.cpp` — `kBase64Lookup` had a runtime-init helper
  flagged by `bugprone-throwing-static-initialization`. Helper is now
  `constexpr` + `noexcept`, lookup is `constexpr`, initialisation is
  compile-time.

## Gate 2 — clang-tidy

**Status:** P0 clean; P1 partly resolved (see policy doc §6).

| Metric | 2026-05-23 | 2026-06-18 |
|---|---|---|
| User-code `.cpp` files | 182 | 468 |
| Unique user-code warnings (`file:line:col:check`) | 338 | 686 |

Detailed breakdown, P0 disposition, and the re-measure command are in
[`clang-tidy-policy.md`](clang-tidy-policy.md#6--baseline-2026-06-18).

The dominant raw check is `cppcoreguidelines-pro-type-member-init`
(1 865 raw, P2 — non-blocking style). Real P1 work remaining:

- 31× `bugprone-empty-catch`
- 6× `bugprone-derived-method-shadowing-base-method` in
  `libs/ensemble/include/euxis/ensemble/providers/{claude,gemini,openai}.hpp`
- 3× `bugprone-narrowing-conversions`
- 1× `bugprone-implicit-widening-of-multiplication-result`
- `bugprone-branch-clone` × 72 — already governed by policy §4 + #55.

## Gate 3 — gcovr line coverage

**Status:** below the 98 % aspirational gate. Overall 79.7 % lines.

| Scope | Lines | Functions | Branches |
|---|---|---|---|
| `apps/` + `libs/` (excluding tests, ETX main, vendor) | **79.7 %** (25 199 / 31 604) | 84.6 % (2 562 / 3 029) | 42.5 % (33 208 / 78 128) |
| `apps/` alone | 73.0 % (14 229 / 19 497) | 77.9 % | 39.1 % |
| `libs/` alone | 90.6 % (10 970 / 12 107) | 90.5 % | 51.5 % |

`libs/` is close to the gate; the gap is concentrated in `apps/` — most
of it in surface that runs forked subprocesses, network requests, or
interactive TUI flows that the current ctest run cannot drive in-process.
`apps/etx/src/main.cpp` is excluded by the existing `cpp-coverage`
target (TUI entry point).

### How to reproduce

```bash
make cpp-coverage   # configures cmake-build/, runs ctest, runs gcovr
```

Or against this branch's `cmake-build-cov/`:

```bash
ctest --test-dir cmake-build-cov -j --output-on-failure
.venv/bin/gcovr --root . --filter 'apps/' --filter 'libs/' \
    --exclude '.*tests/.*' --exclude '.*/build/.*' \
    --exclude '.*/cmake-build-cov/_deps/.*' \
    --exclude 'apps/etx/src/main.cpp' \
    --html-details coverage-report/index.html \
    --json coverage-report/coverage.json \
    --print-summary
```

The `--fail-under-line 98` flag in the `cpp-coverage` Makefile target
will fail until the apps/ surface picks up its missing 18-20 %.
