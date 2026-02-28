# SPDX-License-Identifier: AGPL-3.0-or-later

"""Execution adapter bridging core contracts to the Extism runtime."""

from __future__ import annotations

import time
from pathlib import Path

from euxis_core.contracts import AgentExecutionRequest, AgentExecutionResult
from euxis_core.mesh.extism_runner import WasmExecutor


class ExtismExecutionAdapter:
    """Contract-compliant runtime adapter used by orchestration layers."""

    def __init__(self, plugin_path: str | Path, wasi: bool = True):
        self._executor = WasmExecutor(plugin_path=plugin_path, wasi=wasi)

    def execute(self, request: AgentExecutionRequest) -> AgentExecutionResult:
        start = time.perf_counter()
        try:
            output = self._executor.call(request.function_name, dict(request.payload))
            latency_ms = int((time.perf_counter() - start) * 1000)
            return AgentExecutionResult(ok=True, output=output, latency_ms=latency_ms)
        except Exception as exc:  # pragma: no cover - external runtime behavior
            latency_ms = int((time.perf_counter() - start) * 1000)
            return AgentExecutionResult(ok=False, error=str(exc), latency_ms=latency_ms)
