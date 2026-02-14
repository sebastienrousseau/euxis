# ERROR-001: Error Handling Strategy

## Pattern
Ensure consistent, recoverable, and informative error handling across all system boundaries.

## Severity
HIGH

## Detection Rules
1. **Silent swallowing:** Empty catch blocks, bare `except: pass`, or `|| true` hiding actionable errors — every catch must log, re-raise, or handle
2. **Inconsistent error types:** Mixing string errors, numeric codes, and exception classes in the same module — use a consistent error hierarchy
3. **Missing error boundaries:** External calls (API, database, filesystem) without try/catch or error propagation — all I/O must have explicit error paths
4. **Leaking internals:** Stack traces, internal paths, or SQL queries exposed in user-facing error messages — sanitize before displaying
5. **No retry distinction:** Treating transient errors (network timeout) the same as permanent errors (invalid input) — classify errors as retryable vs. fatal

## Validation
- [ ] No empty catch/except blocks anywhere in the codebase
- [ ] Error hierarchy defined with base class and domain-specific subclasses
- [ ] All external I/O wrapped in error boundaries with explicit handling
- [ ] User-facing errors sanitized (no stack traces, internal paths, or secrets)
- [ ] Transient vs. permanent errors distinguished with appropriate retry logic

## Remediation
- Define an error taxonomy: `AppError > {ValidationError, IOError, AuthError, ...}`
- Use error codes for programmatic handling, messages for humans
- Implement circuit breakers for external service calls
- Log full context at error origin, summarize at error boundary
- Return structured error objects: `{code, message, retryable, context}`
