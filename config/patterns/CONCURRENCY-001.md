# CONCURRENCY-001: Race Condition and Thread Safety Detection

## Pattern
Detect race conditions, deadlocks, mutex misuse, async/await pitfalls, and shared-state corruption in concurrent code.

## Severity
CRITICAL

## Detection Rules
1. **Unprotected shared state:** Global or shared variables accessed from multiple threads/goroutines/coroutines without synchronization (mutex, lock, atomic)
2. **Deadlock potential:** Multiple locks acquired in inconsistent order across different code paths — always acquire locks in a globally consistent order
3. **Async/await pitfalls:** Blocking calls (sleep, sync I/O) inside async functions; missing `await` on promises/coroutines; fire-and-forget async without error handling
4. **TOCTOU races:** Time-of-check to time-of-use gaps where a condition may change between check and action (e.g., `if file exists then read file`)
5. **Unbound concurrency:** Spawning unlimited threads/goroutines/tasks without a semaphore, pool, or backpressure mechanism

## Validation
- [ ] All shared mutable state protected by a lock, atomic, or channel
- [ ] Lock acquisition order is consistent and documented
- [ ] No blocking I/O inside async contexts
- [ ] All promises/futures awaited or explicitly detached with error handling
- [ ] Concurrency bounded by pool size, semaphore, or rate limiter

## Remediation
- Use channels or message-passing over shared mutable state
- Prefer lock-free data structures (atomics, CAS) for simple counters
- Use `asyncio.to_thread()` or equivalent for blocking I/O in async contexts
- Apply the lock hierarchy pattern: assign a global ordering to locks
- Use bounded thread/worker pools with configurable limits
