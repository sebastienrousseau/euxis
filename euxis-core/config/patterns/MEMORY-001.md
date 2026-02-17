# MEMORY-001: Memory and Resource Management

## Pattern
Detect memory leaks, excessive allocations, and resource exhaustion.

## Severity
HIGH

## Detection Rules
1. **Unbounded growth:** Collections that grow without limits (logs, caches, queues)
2. **Missing cleanup:** Resources opened but never closed (file handles, connections, temp files)
3. **Large string operations:** Repeated concatenation in loops (use arrays/builders instead)
4. **Fork bombs:** Recursive process spawning without limits
5. **Temp file leaks:** mktemp without corresponding cleanup/trap

## Validation
- [ ] All caches have TTL or max-size eviction
- [ ] File handles and connections are closed in finally/trap blocks
- [ ] String building uses efficient patterns (arrays in bash, StringBuilder in Java)
- [ ] Process spawning has bounded recursion
- [ ] Trap handlers clean up temp files on EXIT

## Remediation
- Use `trap cleanup EXIT` for bash temp files
- Set max sizes on all in-memory collections
- Use streaming for large data (don't load full file into variable)
- Profile memory with `ps`, `top`, or language-specific tools
- Prefer `read -r line` loops over `$(cat file)` for large files
