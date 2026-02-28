# SPDX-License-Identifier: AGPL-3.0-or-later

"""Resilience primitives for retries and circuit breaking."""

from __future__ import annotations

import random
import time
from dataclasses import dataclass
from typing import Callable, TypeVar

T = TypeVar("T")


@dataclass(slots=True)
class RetryPolicy:
    max_attempts: int = 3
    base_delay_seconds: float = 0.05
    max_delay_seconds: float = 1.0
    jitter_ratio: float = 0.1

    def sleep_duration(self, attempt: int) -> float:
        raw = min(self.base_delay_seconds * (2 ** max(0, attempt - 1)), self.max_delay_seconds)
        jitter = raw * self.jitter_ratio
        return max(0.0, raw + random.uniform(-jitter, jitter))


@dataclass(slots=True)
class CircuitBreaker:
    failure_threshold: int = 5
    recovery_timeout_seconds: float = 10.0
    failure_count: int = 0
    opened_at: float | None = None

    def is_open(self) -> bool:
        if self.opened_at is None:
            return False
        if (time.time() - self.opened_at) >= self.recovery_timeout_seconds:
            self.reset()
            return False
        return True

    def record_success(self) -> None:
        self.reset()

    def record_failure(self) -> None:
        self.failure_count += 1
        if self.failure_count >= self.failure_threshold:
            self.opened_at = time.time()

    def reset(self) -> None:
        self.failure_count = 0
        self.opened_at = None


def run_with_resilience(
    operation: Callable[[], T],
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
            result = operation()
            breaker.record_success()
            return result
        except Exception as exc:  # pragma: no cover - branch covered by tests
            last_error = exc
            breaker.record_failure()
            if breaker.is_open():
                break
            if attempt < retry.max_attempts:
                time.sleep(retry.sleep_duration(attempt))

    if last_error is None:
        raise RuntimeError("Operation failed without exception")
    raise RuntimeError(f"Operation failed after retries: {last_error}") from last_error
