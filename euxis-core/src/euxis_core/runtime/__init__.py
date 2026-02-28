# SPDX-License-Identifier: AGPL-3.0-or-later

"""Runtime adapters and execution backends."""

from .async_bridge import invoke_maybe_async
from .concurrency import gather_awaitables, run_async_with_resilience
from .gateway_ws import GatewayWebSocketClient

__all__ = [
    "GatewayWebSocketClient",
    "gather_awaitables",
    "invoke_maybe_async",
    "run_async_with_resilience",
]
