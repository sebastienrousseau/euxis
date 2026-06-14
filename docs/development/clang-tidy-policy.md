# clang-tidy policy

Canonical reference for the four `phase-1-recon` clean-up issues
filed in May 2026 (#42 epic, #55 P2, #56 P3 concurrency-mt-unsafe,
#57 P3 perf). This document records the policy decisions the team
has agreed to; the individual code fixes are tracked by per-issue
PRs that close each item.

## 1 — `concurrency-mt-unsafe` (issue #56)

### Policy

`concurrency-mt-unsafe` flags POSIX / C-library functions that are
not reentrant (`std::getenv`, `::setenv`, `std::localeconv`,
`::strerror`, …). The check has 110 hits, mostly concentrated in
startup paths that run on the main thread only. The team has
agreed:

- **In `libs/`**: every site annotated with `// NOLINT(concurrency-mt-unsafe)`
  AND a one-line justification, or refactored to use a thread-safe
  alternative (`::getenv_s`, `::strerror_r`, …). No blanket
  disable.
- **In `apps/cli/` and `apps/gateway/`**: provably-single-threaded
  init code is allowed to use the unsafe APIs with a single
  `// NOLINTBEGIN(concurrency-mt-unsafe)` / `// NOLINTEND` block at
  the top of the file, with a comment reference to this doc.
- **In tests**: blanket disable is acceptable — tests do not ship.

### Why annotate, don't disable globally

The non-reentrant functions are legitimate hazards once the codebase
adds a second thread anywhere. We want the check ON so a real
violation produces a build failure rather than a silent regression.

### Refactor candidates (not gating)

`std::getenv` is the dominant offender. A future pass may introduce
`euxis::platform::env_var(const char* name) -> std::optional<std::string>`
in `libs/platform/` that wraps the read in a once-per-process mutex
and returns a defensive copy. Until then, every site stays
annotated.

### Sample annotation

```cpp
const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe) — main-thread init only
```

## 2 — `performance-unnecessary-copy-initialization` (issue #57)

### Policy

`auto x = container.at(key)` and similar accessor copies are valid
when the caller intends to mutate `x`. They are unnecessary when
`x` is read-only inside the scope. The check is correct; the noise
floor is contributors writing `auto` reflexively.

### Acceptable patterns

```cpp
// (1) read-only — use const reference
const auto& cwe = finding.cwe_id;

// (2) read+mutate — explicit copy
auto cwe_lower = finding.cwe_id;
std::transform(cwe_lower.begin(), cwe_lower.end(),
               cwe_lower.begin(), ::tolower);

// (3) trivial type (enum, size_t, string_view) — keep copy
for (auto category : owasp_categories) {  // OwaspCategory is enum
    /* ... */
}
```

### When the check fires false positives

Pattern (3) above does not fire clang-tidy. The 21 sites in #57 are
**non-trivial** copies; do not "fix" the 4 sample sites I checked
(`apps/cli/src/{sarif.cpp:114, box.cpp:88, cmd/scan.cpp:205,
tui/palette.cpp:218}`) because they are pattern (3) (enum, size_t,
string_view, intentional mutation).

### How to find — and fix — the real sites

Two scoped Make targets do this in one command each:

```bash
make cpp-clang-tidy-perf       # read-only survey — prints flagged sites
make cpp-clang-tidy-perf-fix   # applies clang-tidy --fix in place
```

Both targets wrap `scripts/quality/clang_tidy_perf.sh`, which:

1. Requires `clang-tidy` in `$PATH` (LLVM 17+) and a configured
   build (`build/cmake-build/compile_commands.json` — written by
   `make cpp-configure`).
2. Scans every `.cpp` under `apps/` + `libs/` (mirrors the existing
   `cpp-clang-tidy` target's query).
3. Restricts the check list to `-*,performance-unnecessary-copy-initialization`
   so `--fix` only touches this one check.

### Reviewing the fix diff

After `make cpp-clang-tidy-perf-fix`:

```bash
git diff --stat
```

Walk the diff against the patterns enumerated above. Revert any
**pattern (3)** site clang-tidy misidentified (enum, `size_t`,
`string_view`, or intentional mutation paths). The four known
false-positive sites the team identified during issue #57 triage —
`apps/cli/src/{sarif.cpp:114, box.cpp:88, cmd/scan.cpp:205, tui/palette.cpp:218}` —
are pattern (3); if they appear in the diff, drop them.

The expected genuine fix is `auto x =` → `const auto& x =` in roughly
21 cases. Any case that's actually **pattern (2)** (read+mutate)
should be reverted **and** annotated with a one-line comment
explaining the mutation, so a future contributor doesn't re-apply
the fix.

### Closing the loop

After the diff is reviewed and the build + tests pass:

```bash
make cpp-build && make cpp-test
git commit -m "fix(cpp): remediate performance-unnecessary-copy-initialization

Closes #57"
```

CI continues to enforce `performance-unnecessary-copy-initialization`
via the unrestricted `cpp-clang-tidy` target, so any regression after
closure produces a build failure rather than silently re-accumulating.

## 3 — `performance-move-const-arg` and `bugprone-suspicious-stringview-data-usage` (issue #57 cont'd)

### Policy

- `std::move` on a `const` lvalue is always a bug — it silently
  becomes a copy. Fix by either dropping the `move()` or removing
  the `const`.
- `string_view::data()` returns a pointer to a buffer that is **not**
  NUL-terminated. Passing it to a C API that expects a C string
  truncates or reads past the end. Fix by materialising the
  `string_view` to a `std::string` first.

Both checks are unambiguous and should be left ON without
`NOLINT` exceptions.

## 4 — `bugprone-branch-clone` and `bugprone-unused-local-non-trivial-variable` (issue #55)

### Policy

- `bugprone-branch-clone`: review each site. Three legitimate
  patterns exist:
  1. **Copy-paste bug** — fix by merging the branches.
  2. **Future divergence** — annotate with `// NOLINT — intentional dead branch, see <ticket>`.
  3. **Test fixture** that demonstrates the pattern (e.g.
     `libs/cpg/tests/test_cfg.cpp:118` — testing that the CFG
     builder produces the right edges for an if/else that branches
     to identical code paths). Tests can use the blanket
     test-files disable in `.clang-tidy`.
- `bugprone-unused-local-non-trivial-variable`: usually `std::lock_guard`
  / `spdlog::stopwatch` RAII guards. Three options per site:
  1. Use an attribute: `[[maybe_unused]] std::lock_guard<std::mutex> lk(mu);`
  2. Rename to `[[maybe_unused]] auto _ = …` (clang-tidy ignores `_`).
  3. Refactor so the RAII type is actually visible (e.g. take a
     reference to it from the constructor).

## 5 — Issue #60: registry JSON parse perf

### Status

Issue #60 surfaced that `nlohmann::json::parse` of the 53 agent
registry JSON files is ~5.7 % of CLI startup CPU. The chosen
remediation is a content-keyed cache: parse once, persist the
result, re-use on warm cache.

The cache primitive ships in `libs/cache/include/euxis/cache/json_cache.hpp`.
The `RegistryClient` swap-in is a follow-up PR — see the JsonCache
header for the call shape.

The simdjson alternative was considered and rejected: a 3-5×
parser speedup still pays the parse cost on every CLI invocation,
while the cache eliminates it. simdjson can be revisited as a
separate optimisation if the cold-cache cost grows.

## See also

- Issue [#42](https://github.com/sebastienrousseau/euxis/issues/42) — Phase-1 reconnaissance epic
- Issue [#55](https://github.com/sebastienrousseau/euxis/issues/55) — branch-clone + unused-locals
- Issue [#56](https://github.com/sebastienrousseau/euxis/issues/56) — concurrency-mt-unsafe
- Issue [#57](https://github.com/sebastienrousseau/euxis/issues/57) — perf clang-tidy mechanical
- Issue [#60](https://github.com/sebastienrousseau/euxis/issues/60) — registry JSON parse perf
- [`.clang-tidy`](../../.clang-tidy) — check list and tuning
- [`libs/cache/include/euxis/cache/json_cache.hpp`](../../libs/cache/include/euxis/cache/json_cache.hpp) — JsonCache primitive
