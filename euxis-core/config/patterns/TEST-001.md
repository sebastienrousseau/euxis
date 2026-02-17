# TEST-001: Testing Best Practices

## Pattern
Enforce reliable, isolated, and maintainable test suites with meaningful coverage.

## Severity
MEDIUM

## Detection Rules
1. **Shared mutable state:** Tests that depend on execution order or share global state (database rows, files, environment variables) without cleanup — each test must be independently runnable
2. **Excessive mocking:** Mocking more than 2 layers deep or mocking the system under test — mock at boundaries only (I/O, external services), never internal logic
3. **Missing edge cases:** Tests that only cover the happy path — every function with branching logic must have tests for error paths, boundary values, and empty inputs
4. **Flaky indicators:** Tests using `sleep()`, real network calls, system clock, or non-deterministic ordering — use dependency injection, fakes, and deterministic seeds
5. **Assertion-free tests:** Tests that call code but never assert behavior — every test must have at least one meaningful assertion about the result or side effect

## Validation
- [ ] Each test runs independently (random order execution passes)
- [ ] Mocks limited to I/O boundaries; no mocking of internal methods
- [ ] Error paths, boundary values, and empty inputs tested for critical functions
- [ ] No `sleep()`, real network, or system clock in unit tests
- [ ] Every test contains at least one meaningful assertion

## Remediation
- Use setup/teardown hooks for state isolation (fresh DB, temp dirs, env reset)
- Replace mocks with fakes for complex dependencies (in-memory DB, stub server)
- Use property-based testing for functions with wide input domains
- Run tests in random order (pytest-randomly, jest --randomize) to catch coupling
- Measure mutation testing score alongside line coverage for true coverage quality
