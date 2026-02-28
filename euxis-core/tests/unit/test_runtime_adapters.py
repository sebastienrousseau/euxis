# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import asyncio
from pathlib import Path

import pytest

import euxis_core.runtime.async_bridge as async_bridge
import euxis_core.runtime.gateway_ws as gateway_ws
from euxis_core.contracts import AgentExecutionRequest
from euxis_core.platform import LocalFilesystemAdapter, RuntimePlatformAdapter
from euxis_core.runtime import (
    GatewayWebSocketClient,
    gather_awaitables,
    invoke_maybe_async,
    run_async_with_resilience,
)
from euxis_core.runtime.wasm_adapter import ExtismExecutionAdapter
from euxis_core.resilience import CircuitBreaker, RetryPolicy


def test_platform_name_mappings(monkeypatch) -> None:
    monkeypatch.setattr("euxis_core.platform.adapters.py_platform.system", lambda: "Linux")
    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    assert RuntimePlatformAdapter().platform_name() == "linux"

    monkeypatch.setattr("euxis_core.platform.adapters.py_platform.system", lambda: "Windows")
    assert RuntimePlatformAdapter().platform_name() == "windows"

    monkeypatch.setattr("euxis_core.platform.adapters.py_platform.system", lambda: "Solaris")
    assert RuntimePlatformAdapter().platform_name() == "unknown"


def test_platform_is_wsl_proc_version(monkeypatch, tmp_path: Path) -> None:
    version_file = tmp_path / "version"
    version_file.write_text("Linux version 6.1.0 Microsoft", encoding="utf-8")

    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    monkeypatch.setattr("euxis_core.platform.adapters.Path", lambda *_: version_file)
    assert RuntimePlatformAdapter().is_wsl() is True


def test_platform_is_wsl_oserror(monkeypatch) -> None:
    class BrokenPath:
        def exists(self) -> bool:
            raise OSError("permission denied")

    monkeypatch.delenv("WSL_DISTRO_NAME", raising=False)
    monkeypatch.setattr("euxis_core.platform.adapters.Path", lambda *_: BrokenPath())
    assert RuntimePlatformAdapter().is_wsl() is False


def test_local_filesystem_adapter_roundtrip(tmp_path: Path) -> None:
    fs = LocalFilesystemAdapter()
    folder = fs.ensure_dir(tmp_path / "a" / "b")
    assert folder.exists()
    target = tmp_path / "a" / "b" / "f.txt"
    fs.write_text(target, "ok")
    assert fs.read_text(target) == "ok"


@pytest.mark.asyncio
async def test_gather_awaitables() -> None:
    async def one() -> int:
        return 1

    async def two() -> int:
        return 2

    values = await gather_awaitables([one(), two()])
    assert values == [1, 2]


@pytest.mark.asyncio
async def test_run_async_with_resilience_open_breaker() -> None:
    breaker = CircuitBreaker(failure_threshold=1, recovery_timeout_seconds=60.0)
    breaker.record_failure()
    with pytest.raises(RuntimeError, match="Circuit breaker is open"):
        await run_async_with_resilience(lambda: asyncio.sleep(0), circuit_breaker=breaker)


@pytest.mark.asyncio
async def test_run_async_with_resilience_last_error_none() -> None:
    with pytest.raises(RuntimeError, match="without exception"):
        await run_async_with_resilience(
            lambda: asyncio.sleep(0),
            retry_policy=RetryPolicy(max_attempts=0, base_delay_seconds=0.0, jitter_ratio=0.0),
        )


def test_invoke_maybe_async_sync_handler() -> None:
    def handler(payload: dict[str, int]) -> dict[str, int]:
        return {"ok": payload["v"]}

    assert invoke_maybe_async(handler, {"v": 3}) == {"ok": 3}


def test_invoke_maybe_async_async_handler() -> None:
    async def handler(payload: dict[str, int]) -> dict[str, int]:
        return {"ok": payload["v"]}

    assert invoke_maybe_async(handler, {"v": 4}) == {"ok": 4}


def test_invoke_maybe_async_fallback_loop(monkeypatch) -> None:
    calls = {"closed": False}
    real_new_event_loop = asyncio.new_event_loop

    class FakeLoop:
        def run_until_complete(self, awaitable):
            loop = real_new_event_loop()
            try:
                return loop.run_until_complete(awaitable)
            finally:
                loop.close()

        def close(self):
            calls["closed"] = True

    async def handler(payload: dict[str, int]) -> dict[str, int]:
        return {"ok": payload["v"]}

    monkeypatch.setattr(async_bridge.asyncio, "run", lambda _coro: (_ for _ in ()).throw(RuntimeError()))
    monkeypatch.setattr(async_bridge.asyncio, "new_event_loop", lambda: FakeLoop())
    assert invoke_maybe_async(handler, {"v": 5}) == {"ok": 5}
    assert calls["closed"] is True


@pytest.mark.asyncio
async def test_gateway_ws_client_flow(monkeypatch) -> None:
    class FakeWS:
        def __init__(self):
            self.closed = False
            self.sent: list[str] = []

        async def send(self, message: str) -> None:
            self.sent.append(message)

        def __aiter__(self):
            async def _gen():
                yield "m1"
                yield "m2"

            return _gen()

        async def close(self):
            self.closed = True

    fake_ws = FakeWS()

    async def fake_connect(url: str, additional_headers=None):
        assert url == "ws://gateway"
        assert additional_headers["Authorization"] == "Bearer tok"
        return fake_ws

    monkeypatch.setattr(gateway_ws.websockets, "connect", fake_connect)

    client = GatewayWebSocketClient("ws://gateway", token="tok")
    assert client.closed is True
    await client.connect()
    assert client.closed is False
    await client.send("hello")
    got = []
    async for msg in client.iter_messages():
        got.append(msg)
    assert got == ["m1", "m2"]
    await client.close()
    assert client.closed is True
    assert fake_ws.sent == ["hello"]


@pytest.mark.asyncio
async def test_gateway_ws_connect_without_token_and_reuse(monkeypatch) -> None:
    calls = {"count": 0}

    class FakeWS:
        def __init__(self):
            self.closed = False

    fake_ws = FakeWS()

    async def fake_connect(url: str, additional_headers=None):
        calls["count"] += 1
        assert url == "ws://gateway"
        assert additional_headers == {}
        return fake_ws

    monkeypatch.setattr(gateway_ws.websockets, "connect", fake_connect)
    client = GatewayWebSocketClient("ws://gateway")
    await client.connect()
    await client.connect()
    assert calls["count"] == 1


@pytest.mark.asyncio
async def test_gateway_ws_client_errors() -> None:
    client = GatewayWebSocketClient("ws://gateway")
    with pytest.raises(RuntimeError, match="not connected"):
        await client.send("x")
    with pytest.raises(RuntimeError, match="not connected"):
        async for _ in client.iter_messages():
            pass


def test_gateway_invalid_status_code() -> None:
    class FakeInvalidStatusCode(Exception):
        def __init__(self, code):
            self.status_code = code

    class FakeResponse:
        def __init__(self, code):
            self.status_code = code

    class FakeResponseStatus(Exception):
        def __init__(self, code):
            self.response = FakeResponse(code)

    assert GatewayWebSocketClient.invalid_status_code(FakeInvalidStatusCode(401)) == 401
    assert GatewayWebSocketClient.invalid_status_code(FakeInvalidStatusCode("403")) == 403
    assert GatewayWebSocketClient.invalid_status_code(FakeResponseStatus(429)) == 429
    assert GatewayWebSocketClient.invalid_status_code(FakeInvalidStatusCode("bad")) is None
    assert GatewayWebSocketClient.invalid_status_code(RuntimeError("x")) is None


def test_extism_execution_adapter_success(monkeypatch) -> None:
    class FakeExecutor:
        def __init__(self, plugin_path, wasi):
            self.plugin_path = plugin_path
            self.wasi = wasi

        def call(self, function_name, payload):
            assert function_name == "run"
            assert payload == {"v": 1}
            return {"ok": True}

    monkeypatch.setattr("euxis_core.runtime.wasm_adapter.WasmExecutor", FakeExecutor)
    adapter = ExtismExecutionAdapter("agent.wasm", wasi=True)
    result = adapter.execute(AgentExecutionRequest("a1", "run", {"v": 1}))
    assert result.ok is True
    assert result.output == {"ok": True}


def test_extism_execution_adapter_failure(monkeypatch) -> None:
    class FakeExecutor:
        def __init__(self, plugin_path, wasi):
            self.plugin_path = plugin_path
            self.wasi = wasi

        def call(self, function_name, payload):
            raise ValueError("boom")

    monkeypatch.setattr("euxis_core.runtime.wasm_adapter.WasmExecutor", FakeExecutor)
    adapter = ExtismExecutionAdapter("agent.wasm", wasi=True)
    result = adapter.execute(AgentExecutionRequest("a1", "run", {"v": 1}))
    assert result.ok is False
    assert "boom" in (result.error or "")
