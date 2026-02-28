# SPDX-License-Identifier: AGPL-3.0-or-later

"""Runtime concurrency helpers.

Centralizes event-loop primitives outside pure orchestration modules.
"""

from __future__ import annotations

import asyncio
from typing import Any, Awaitable, Callable, Iterable, TypeVar

from euxis_core.resilience import CircuitBreaker, RetryPolicy

T = TypeVar("T")


async def gather_awaitables(awaitables: Iterable[Awaitable[Any]]) -> list[Any]:
    return list(await asyncio.gather(*awaitables))


async def run_async_with_resilience(
    operation: Callable[[], Awaitable[T]],
    retry_policy: RetryPolicy | None = None,
    circuit_breaker: CircuitBreaker | None = None,
) -> T:
    retry = retry_policy or RetryPolicy()
    breaker = circuit_breaker or CircuitBreaker()

    if breaker.is_open():
        raise RuntimeError("Circuit breaker is open")

    last_error: Exception | None = None
    for attempt in range(1, retry.max_attempts + 1):
        try:
            result = await operation()
            breaker.record_success()
            return result
        except Exception as exc:  # pragma: no cover - exercised in unit tests
            last_error = exc
            breaker.record_failure()
            if breaker.is_open():
                break
            if attempt < retry.max_attempts:
                await asyncio.sleep(retry.sleep_duration(attempt))

    if last_error is None:
        raise RuntimeError("Operation failed without exception")
    raise RuntimeError(f"Operation failed after retries: {last_error}") from last_error
