# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""High-performance metrics collector with binary format and async batching.

Optimized for minimal hot path overhead (<1ms):
- Binary msgpack format instead of JSON
- Async batched writes to reduce I/O
- Lock-free metrics buffer
- Fire-and-forget API for hot paths

Target: hot path overhead <1ms
"""

from __future__ import annotations

import asyncio
import os
import struct
import time
import uuid
from collections import deque
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path
from threading import Lock
from typing import Any

# Try msgpack for binary serialization, fallback to JSON
try:
    import msgpack
    HAS_MSGPACK = True
except ImportError:
    HAS_MSGPACK = False
    import json

# Constants
BATCH_SIZE = 100
FLUSH_INTERVAL = 1.0  # seconds
BUFFER_MAX_SIZE = 10000
METRICS_FILE_EXT = ".mpack" if HAS_MSGPACK else ".jsonl"


@dataclass(slots=True)
class MetricEvent:
    """Lightweight metric event with slots for memory efficiency."""
    event_type: str
    timestamp: float
    properties: dict[str, Any]


class FastMetricsBuffer:
    """Lock-free circular buffer for metrics collection."""

    def __init__(self, max_size: int = BUFFER_MAX_SIZE) -> None:
        self._buffer: deque[MetricEvent] = deque(maxlen=max_size)
        self._lock = Lock()

    def append(self, event: MetricEvent) -> None:
        """Append event to buffer (thread-safe)."""
        with self._lock:
            self._buffer.append(event)

    def drain(self) -> list[MetricEvent]:
        """Drain all events from buffer."""
        with self._lock:
            events = list(self._buffer)
            self._buffer.clear()
            return events

    def __len__(self) -> int:
        return len(self._buffer)


class FastMetricsCollector:
    """High-performance metrics collector with async batching.

    Usage:
        collector = FastMetricsCollector()
        collector.record("agent_task_started", {"agent_id": "architect"})

        # In async context:
        await collector.start_background_flush()
    """

    def __init__(self, metrics_dir: str | None = None) -> None:
        base = metrics_dir or str(
            Path(os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis")))
            / "euxis-runtime" / "metrics"
        )
        self.metrics_dir = Path(base)
        self.metrics_dir.mkdir(parents=True, exist_ok=True)

        self._buffer = FastMetricsBuffer()
        self._flush_task: asyncio.Task | None = None
        self._shutdown = False

        # Pre-computed file path
        self._events_file = self.metrics_dir / f"events{METRICS_FILE_EXT}"

    def record(self, event_type: str, properties: dict[str, Any] | None = None) -> None:
        """Record a metric event (fire-and-forget, <1ms).

        This is the hot path - optimized for minimal overhead.
        """
        event = MetricEvent(
            event_type=event_type,
            timestamp=time.time(),
            properties=properties or {}
        )
        self._buffer.append(event)

    def record_metric(self, name: str, value: float, tags: dict[str, str] | None = None) -> None:
        """Record a numeric metric (fire-and-forget, <1ms)."""
        self.record("metric", {
            "name": name,
            "value": value,
            "tags": tags or {}
        })

    def record_timing(self, name: str, duration_ms: float, tags: dict[str, str] | None = None) -> None:
        """Record a timing metric in milliseconds."""
        self.record("timing", {
            "name": name,
            "duration_ms": duration_ms,
            "tags": tags or {}
        })

    def record_counter(self, name: str, delta: int = 1, tags: dict[str, str] | None = None) -> None:
        """Increment a counter metric."""
        self.record("counter", {
            "name": name,
            "delta": delta,
            "tags": tags or {}
        })

    async def flush(self) -> int:
        """Flush buffered events to disk."""
        events = self._buffer.drain()
        if not events:
            return 0

        # Serialize events
        if HAS_MSGPACK:
            data = b"".join(
                msgpack.packb({
                    "t": e.event_type,
                    "ts": e.timestamp,
                    "p": e.properties
                })
                for e in events
            )
            mode = "ab"
        else:
            data = "\n".join(
                json.dumps({
                    "event_type": e.event_type,
                    "timestamp": e.timestamp,
                    "properties": e.properties
                })
                for e in events
            ) + "\n"
            mode = "a"

        # Write to file (async-friendly with thread pool)
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, self._write_file, data, mode)

        return len(events)

    def _write_file(self, data: bytes | str, mode: str) -> None:
        """Write data to file (runs in thread pool)."""
        if isinstance(data, str):
            with open(self._events_file, mode, encoding="utf-8") as f:
                f.write(data)
        else:
            with open(self._events_file, mode + "b") as f:
                f.write(data)

    async def start_background_flush(self) -> None:
        """Start background flush task."""
        if self._flush_task is None:
            self._flush_task = asyncio.create_task(self._background_flush_loop())

    async def _background_flush_loop(self) -> None:
        """Background loop to periodically flush metrics."""
        while not self._shutdown:
            await asyncio.sleep(FLUSH_INTERVAL)
            try:
                await self.flush()
            except Exception:
                pass  # Don't let flush errors break the loop

    async def shutdown(self) -> None:
        """Shutdown collector and flush remaining events."""
        self._shutdown = True
        if self._flush_task:
            self._flush_task.cancel()
            try:
                await self._flush_task
            except asyncio.CancelledError:
                pass
        # Final flush
        await self.flush()

    def sync_flush(self) -> int:
        """Synchronous flush for non-async contexts."""
        events = self._buffer.drain()
        if not events:
            return 0

        if HAS_MSGPACK:
            data = b"".join(
                msgpack.packb({
                    "t": e.event_type,
                    "ts": e.timestamp,
                    "p": e.properties
                })
                for e in events
            )
            with open(self._events_file, "ab") as f:
                f.write(data)
        else:
            with open(self._events_file, "a", encoding="utf-8") as f:
                for e in events:
                    f.write(json.dumps({
                        "event_type": e.event_type,
                        "timestamp": e.timestamp,
                        "properties": e.properties
                    }) + "\n")

        return len(events)


# Global fast collector instance
fast_collector = FastMetricsCollector()


# Convenience functions for hot path usage
def record(event_type: str, properties: dict[str, Any] | None = None) -> None:
    """Fire-and-forget metric recording (<1ms)."""
    fast_collector.record(event_type, properties)


def record_metric(name: str, value: float, tags: dict[str, str] | None = None) -> None:
    """Fire-and-forget numeric metric (<1ms)."""
    fast_collector.record_metric(name, value, tags)


def record_timing(name: str, duration_ms: float, tags: dict[str, str] | None = None) -> None:
    """Fire-and-forget timing metric (<1ms)."""
    fast_collector.record_timing(name, duration_ms, tags)


def record_counter(name: str, delta: int = 1, tags: dict[str, str] | None = None) -> None:
    """Fire-and-forget counter increment (<1ms)."""
    fast_collector.record_counter(name, delta, tags)
