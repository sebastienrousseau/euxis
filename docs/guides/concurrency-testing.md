# Deterministic Concurrency Test Suite

## Overview

This test suite provides comprehensive deterministic testing of concurrency patterns in the Euxis fleet system, covering cancellation, backpressure, ordering, and race conditions.

## Test Coverage

### 1. Stage-Based Execution (`TestConcurrentStageExecution`)
- **Stage ordering deterministic**: Verifies stages execute in correct dependency order
- **Parallel execution within stage**: Confirms agents within same stage run concurrently
- **Dependency resolution**: Tests that `depends_on` relationships are respected

### 2. File Locking (`TestFileLockingConcurrency`)
- **Exclusive lock access**: Only one thread can hold a lock at a time
- **Lock timeout behavior**: Lock acquisition respects timeout values
- **Lock release cleanup**: Proper cleanup when locks are released

### 3. Subprocess Management (`TestSubprocessCancellation`)
- **Cancellation cleanup**: Terminated subprocesses clean up properly
- **Backpressure handling**: Large subprocess output is handled without blocking
- **Process termination**: Graceful and forced termination work correctly

### 4. Async Patterns (`TestAsyncConcurrencyPatterns`)
- **Task cancellation**: `asyncio.CancelledError` propagates correctly
- **Queue backpressure**: Async queues apply backpressure when full
- **Voice pipeline concurrency**: Multiple voice requests process in parallel

### 5. Race Condition Prevention (`TestRaceConditionPrevention`)
- **Counter race conditions**: Proper locking prevents data races
- **Dictionary races**: Concurrent dict access with locking
- **File write races**: Preventing corrupted file writes

### 6. Message Bus Concurrency (`TestMessageBusConcurrency`)
- **Producer-consumer pattern**: Queue-based message passing with backpressure
- **Multiple publishers**: Message ordering with concurrent publishers
- **Queue overflow handling**: Graceful handling when queues reach capacity

## Integration Tests

### Dispatch Integration (`test_dispatch_integration.py`)
- **Real euxis-dispatch testing**: Tests actual bash script concurrency
- **Manifest validation**: Invalid manifests are properly rejected
- **Log file generation**: Agent execution logs are created
- **Failure handling**: Agent failures don't crash the system
- **High concurrency load**: Stress testing with many parallel agents

## Test Files

| File | Purpose | Test Count |
|------|---------|------------|
| `test_deterministic_concurrency.py` | Core concurrency patterns | 15+ tests |
| `test_dispatch_integration.py` | euxis-dispatch integration | 10+ tests |
| `test_dispatch_concurrency.sh` | Existing bash tests | 6 tests |
| `verify_concurrency_tests.py` | Test verification runner | N/A |

## Running Tests

### Quick Verification
```bash
# Run single test class
python3 -m pytest test_deterministic_concurrency.py::TestRaceConditionPrevention -v

# Run all deterministic tests
python3 -m pytest test_deterministic_concurrency.py -v

# Run integration tests
python3 -m pytest test_dispatch_integration.py -v
```

### Comprehensive Verification
```bash
# Run verification suite (3 runs per module)
./verify_concurrency_tests.py

# Quick verification (1 run per module)
./verify_concurrency_tests.py --quick

# Stress testing (5 runs per module)
./verify_concurrency_tests.py --stress
```

### Manual Test Execution
```bash
# Run existing bash tests
./test_dispatch_concurrency.sh

# Verify specific module
python3 test_deterministic_concurrency.py --verify
```

## Deterministic Testing Features

### Mock Dependencies
- **MockFileSystem**: Deterministic file locking simulation
- **Subprocess mocking**: Predictable process behavior
- **Async task control**: Controlled timing for async operations

### Isolation Guarantees
- **No external dependencies**: All network/database calls are mocked
- **Temporary directories**: Tests don't interfere with real files
- **Thread barriers**: Synchronized starts for parallel execution tests
- **Process cleanup**: Automatic cleanup of spawned processes

### Timing Control
- **Deterministic delays**: Known sleep durations for predictable timing
- **Timeout enforcement**: Tests fail fast on hangs
- **Execution time verification**: Parallel operations complete faster than sequential

## Anti-Flaky Patterns

### Race Condition Prevention
- **Thread barriers**: Ensure simultaneous test start
- **Lock verification**: Confirm exclusive access patterns
- **State verification**: Check shared state consistency

### Resource Management
- **Temporary files**: Auto-cleanup prevents interference
- **Process termination**: Forced cleanup on test failure
- **Mock isolation**: No real system resource usage

### Timing Stability
- **Relative timing**: Tests verify relative, not absolute timing
- **Variance tolerance**: Small timing variations are acceptable
- **Retry patterns**: Not used - tests should be deterministic

## Dependencies

### Required Python Packages
- `pytest` - Test framework
- `asyncio` - Async testing (built-in)
- `threading` - Thread testing (built-in)
- `multiprocessing` - Process testing (built-in)
- `concurrent.futures` - Executor testing (built-in)

### System Requirements
- `bash` - For integration tests
- `jq` - For manifest parsing
- `timeout` - For process timeout handling

### Optional Dependencies
- `pytest-asyncio` - Better async test support (recommended)

## Verification Commands

The test suite includes built-in verification commands:

```bash
# Verify all tests pass
euxis-test-infra verify-concurrency

# Command that returns exit 0 on success
python3 -m pytest tests/test_deterministic_concurrency.py -q --tb=no
```

## Expected Behavior

### Success Criteria
- All tests complete within timeout (120s per module)
- Results are identical across multiple runs
- No race conditions detected in stress tests
- All subprocess cleanup completes successfully

### Performance Expectations
- Parallel tests complete faster than sequential equivalents
- Lock contention tests show exactly one winner
- Async cancellation propagates within 100ms
- File operations complete atomically

## Troubleshooting

### Common Issues

**Non-deterministic timing failures:**
- Increase sleep durations in timing-sensitive tests
- Verify system load isn't affecting test timing

**Resource leaks:**
- Check that all threads/processes are properly joined/terminated
- Verify temporary files are cleaned up

**Mock failures:**
- Ensure external dependencies are properly mocked
- Check that mocks return consistent data

**Integration test failures:**
- Verify euxis-dispatch script is executable
- Check that required system tools (jq, bash) are available

### Debug Commands
```bash
# Run with maximum verbosity
python3 -m pytest test_deterministic_concurrency.py -vvv -s

# Debug specific test
python3 -m pytest test_deterministic_concurrency.py::TestClass::test_method -vvv -s

# Check test discovery
python3 -m pytest --collect-only test_deterministic_concurrency.py
```

## Test Maintenance

### Adding New Tests
1. Add test method to appropriate test class
2. Mock all external dependencies
3. Use deterministic timing patterns
4. Add to verification suite if needed

### Updating Existing Tests
1. Maintain deterministic behavior
2. Update verification expectations
3. Test across multiple runs to verify stability

### Performance Monitoring
- Monitor test execution times
- Flag tests taking >30 seconds as slow
- Optimize or parallelize slow test components

---

*Euxis v0.1.2 · Build something that matters.*