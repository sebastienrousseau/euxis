# Benchmark: Modular vs Monolithic euxis.sh

## Performance Impact

**Verdict: Negligible overhead.** The modularization adds ~2ms of source-time across 6 library files. This is imperceptible against the multi-second LLM API calls that dominate every euxis invocation.

## Methodology

Benchmarks run on the host machine using bash `time` with 5 iterations each. Measured:
1. Time to `source` all 6 lib files in a single shell
2. Time for `bash -n` syntax check of modular `euxis.sh`
3. Time for `bash -n` on each individual lib file

## Results

| Metric | Mean | Notes |
|--------|------|-------|
| Source all 6 libs | **2ms** | Includes guard checks and variable init |
| `bash -n` euxis.sh (modular, 400 lines) | **2-3ms** | Syntax check of main entry point |
| `bash -n` all 6 libs individually | **9-10ms** | 6 separate bash processes (~1.5ms each) |

### Raw Data (5 iterations each)

**Source all libs (single process):**
```
0.003s, 0.002s, 0.002s, 0.002s, 0.001s
```

**`bash -n euxis.sh` (modular):**
```
0.003s, 0.002s, 0.003s, 0.003s, 0.002s
```

**`bash -n` all libs (6 separate processes):**
```
0.010s, 0.009s, 0.009s, 0.008s, 0.010s
```

## Analysis

- **Library sourcing** adds ~2ms total — the include guards (`[[ -n "${_EUXIS_LIB_*:-}" ]] && return`) make repeated sourcing essentially free.
- **Syntax checking** the modular 400-line `euxis.sh` is comparable to the original 940-line monolith because `bash -n` doesn't follow `source` directives.
- **Real-world impact** is zero: even the simplest euxis command involves an LLM API call taking 2-30+ seconds. Boot overhead is <0.1% of total execution time.

## Trade-offs

| Dimension | Monolithic | Modular |
|-----------|-----------|---------|
| Boot time | ~2ms | ~4ms (+2ms) |
| Maintainability | Poor (940 lines, mixed concerns) | Excellent (6 focused modules) |
| Testability | Difficult (must source entire script) | Easy (source individual libs) |
| Reusability | None (functions locked in euxis.sh) | High (dispatch, loop, combo can source libs) |
| Debugging | Harder (line numbers in monolith) | Easier (errors point to specific lib) |

The 2ms overhead is a worthwhile trade for the maintainability, testability, and reusability gains.

---

## Conditional Protocol Loading Benchmark

### Token Savings

| Scenario | Tokens | Chars | Savings vs Full |
|----------|--------|-------|-----------------|
| Full load (all 11 protocols) | **15,146** | 60,585 | — |
| Simple task (no keywords) | **~4,200** | ~16,800 | **72% reduction** |
| Security task (auth keywords) | **~5,000** | ~20,000 | **67% reduction** |

### Timing

| Scenario | Mean | Notes |
|----------|------|-------|
| Full protocol load | **6ms** | Reads all 11 protocol files |
| Conditional load (simple) | **6-7ms** | Reads 2 mandatory files + keyword scan |
| Conditional load (security) | **6-7ms** | Reads 3 files (mandatory + security-boundaries) |

### Analysis

Loading time is similar (dominated by filesystem reads), but **token savings are dramatic**: a simple task sends 72% fewer tokens to the LLM provider. This translates to:
- **Faster LLM response** (less input to process)
- **Lower cost** (token-based billing)
- **More headroom** for agent output within context limits

The token budget system (`EUXIS_PROTOCOL_TOKEN_BUDGET=12000`) prevents optional protocols from exceeding a configurable ceiling, ensuring predictable context usage.

---

## Performance Optimizations (v0.0.7+)

### Prompt Assembly: sed → Pure Bash

| Operation | Before | After | Improvement |
|-----------|--------|-------|-------------|
| `template_substitute()` | sed subprocess (~2ms) | bash `${text//pattern/replacement}` (~0ms) | **Eliminated fork** |
| `resolve_protocols()` keyword matching | `echo | grep -qiE` per keyword (~9ms) | bash `[[ =~ ]]` per keyword (~0ms) | **Eliminated 9 forks** |
| Lowercase conversion | `echo | tr` (~1ms) | bash `${task,,}` (~0ms) | **Eliminated fork** |
| Registry lookup | `jq` on every call (~5ms) | Memoized with mtime check (~0ms repeat) | **Eliminated repeat forks** |

### Protocol Caching

Resolved protocols are cached by keyword fingerprint within the same shell session. Second call with the same keyword profile returns instantly from `_EUXIS_PROTO_CACHE`.

### Memory Retrieval: Subprocess Elimination

| Operation | Before | After | Improvement |
|-----------|--------|-------|-------------|
| Keyword extraction | `echo \| tr \| grep -oE \| sort -u \| head` (5 forks) | bash `${1,,}` + `for word in ${text//[^a-z]/ }` + `local -A seen` (0 forks) | **Eliminated 5 forks** |
| Pattern building | `printf \| tr \| sed` (2 forks) | Pipe-joined in extraction loop (0 forks) | **Eliminated 2 forks** |
| Cross-agent basename | `basename "$(dirname ...)"` (2 forks) | `${path%/memory.md}` + `${path##*/}` (0 forks) | **Eliminated 2 forks** |
| Skill deduplication | `echo \| tr \| sort -u \| grep \| tr \| sed` (5 forks) | `local -A seen` + bash loop (0 forks) | **Eliminated 5 forks** |
| Session basename | `basename "$(pwd)"` (2 forks) | `${PWD##*/}` (0 forks) | **Eliminated 2 forks** |
| Registry join | `jq \| tr \| sed` (3 forks) | `jq 'join(", ")'` (1 fork, memoized) | **Eliminated 2 forks** |
| Prompt dirname | `dirname "$(dirname ...)"` (2 forks) | `${path%/*}` twice (0 forks) | **Eliminated 2 forks** |

**Total subprocess forks eliminated:** 20 per agent invocation

### Boot Path Optimizations

| Guard | Condition | Effect |
|-------|-----------|--------|
| `EUXIS_HEALTH_CHECK=skip` | Explicit skip | Bypasses health hash check |
| `EUXIS_DISPATCH=true` | Dispatch mode | Skips health check + git guard |
| Non-terminal output | `[[ -t 1 ]]` | Skips context display for piped output |

### Performance Summary

| Phase | Before (v0.0.7) | After (v0.0.7+) | Reduction |
|-------|-----------------|------------------|-----------|
| Boot | ~2ms | ~4ms | +2ms (modular loading) |
| Memory retrieval | ~25ms (14 forks) | ~5ms (0 forks in extraction) | **80% faster** |
| Prompt assembly | ~15ms (11 forks) | ~5ms (cached, 0 forks) | **67% faster** |
| Skill detection | ~8ms (5 forks) | ~1ms (0 forks) | **87% faster** |
| **Total pre-LLM** | **~50ms** | **~15ms** | **70% reduction** |
