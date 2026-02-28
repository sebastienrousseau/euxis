# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import asyncio
import time

from euxis_core.resilience import CircuitBreaker, RetryPolicy, run_with_resilience
from euxis_core.runtime import run_async_with_resilience


def test_retries_eventually_succeed() -> None:
    attempts = {"count": 0}

    def flaky() -> str:
        attempts["count"] += 1
        if attempts["count"] < 3:
            raise ValueError("transient")
        return "ok"

    result = run_with_resilience(
        flaky,
        retry_policy=RetryPolicy(max_attempts=4, base_delay_seconds=0.0, jitter_ratio=0.0),
    )
    assert result == "ok"
    assert attempts["count"] == 3


def test_circuit_breaker_opens_and_blocks() -> None:
    breaker = CircuitBreaker(failure_threshold=2, recovery_timeout_seconds=0.5)

    def always_fail() -> None:
        raise RuntimeError("fail")

    try:
        run_with_resilience(
            always_fail,
            retry_policy=RetryPolicy(max_attempts=2, base_delay_seconds=0.0, jitter_ratio=0.0),
            circuit_breaker=breaker,
        )
    except RuntimeError:
        pass

    assert breaker.is_open()

    try:
        run_with_resilience(always_fail, retry_policy=RetryPolicy(max_attempts=1), circuit_breaker=breaker)
        assert False, "expected open breaker"
    except RuntimeError as exc:
        assert "open" in str(exc).lower()

    time.sleep(0.6)
    assert not breaker.is_open()


def test_async_retries_eventually_succeed() -> None:
    attempts = {"count": 0}

    async def flaky_async() -> str:
        attempts["count"] += 1
        if attempts["count"] < 3:
            raise ValueError("transient")
        return "ok"

    result = asyncio.run(
        run_async_with_resilience(
            flaky_async,
            retry_policy=RetryPolicy(max_attempts=4, base_delay_seconds=0.0, jitter_ratio=0.0),
        )
    )
    assert result == "ok"
    assert attempts["count"] == 3


def test_run_with_resilience_raises_without_exception_when_no_attempts() -> None:
    def never_called() -> str:
        return "ok"

    try:
        run_with_resilience(
            never_called,
            retry_policy=RetryPolicy(max_attempts=0, base_delay_seconds=0.0, jitter_ratio=0.0),
        )
        assert False, "expected failure"
    except RuntimeError as exc:
        assert "without exception" in str(exc)


def test_async_resilience_raises_after_retries() -> None:
    async def always_fail() -> str:
        raise ValueError("x")

    try:
        asyncio.run(
            run_async_with_resilience(
                always_fail,
                retry_policy=RetryPolicy(max_attempts=2, base_delay_seconds=0.0, jitter_ratio=0.0),
            )
        )
        assert False, "expected retry failure"
    except RuntimeError as exc:
        assert "after retries" in str(exc)
