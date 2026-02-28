# SPDX-License-Identifier: AGPL-3.0-or-later

"""Async invocation bridge for sync runtime callbacks."""

from __future__ import annotations

import asyncio
import inspect
from typing import Any, Callable


def invoke_maybe_async(handler: Callable[[dict[str, Any]], Any], payload: dict[str, Any]) -> Any:
    if inspect.iscoroutinefunction(handler):
        coro = handler(payload)
        try:
            return asyncio.run(coro)
        except RuntimeError:
            coro.close()
            loop = asyncio.new_event_loop()
            try:
                return loop.run_until_complete(handler(payload))
            finally:
                loop.close()
    return handler(payload)
