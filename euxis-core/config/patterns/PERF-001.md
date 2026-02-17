# PERF-001: Hot Path Optimization

## Pattern
Identify and optimize code in the critical execution path (hot paths).

## Severity
HIGH

## Detection Rules
1. **Unnecessary allocations in loops:** Creating objects/arrays inside tight loops
2. **N+1 queries:** Database queries executed per-item instead of batched
3. **Synchronous I/O in async context:** Blocking calls in event loops
4. **Redundant computation:** Same calculation performed multiple times without caching
5. **Excessive subprocess spawns:** Forking processes for operations achievable in-process

## Validation
- [ ] Hot paths identified via profiling (not guessing)
- [ ] Allocations minimized in tight loops
- [ ] Database queries batched where possible
- [ ] Computation results cached when reused
- [ ] Subprocess count minimized (use builtins over forks)

## Remediation
- Profile before optimizing — measure, don't guess
- Batch I/O operations (database, filesystem, network)
- Use memoization for pure functions called with same args
- Prefer bash builtins over external commands in hot paths
- Use `$(< file)` instead of `$(cat file)` in bash
