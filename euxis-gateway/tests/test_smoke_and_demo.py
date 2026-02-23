import asyncio
import sys
import types

import pytest

if "websockets" not in sys.modules:
    sys.modules["websockets"] = types.SimpleNamespace(connect=lambda *_args, **_kwargs: None)

from gateway import smoke_test, demo


class DummyResponse:
    def __init__(self, status_code=200, text="ok"):
        self.status_code = status_code
        self.text = text


def test_smoke_success(monkeypatch, capsys):
    monkeypatch.setattr(smoke_test.httpx, "get", lambda *_args, **_kwargs: DummyResponse())
    monkeypatch.setattr(smoke_test.sys, "argv", ["smoke_test.py"])
    assert smoke_test.main() == 0
    assert "ok" in capsys.readouterr().out


def test_smoke_failure_status(monkeypatch, capsys):
    monkeypatch.setattr(smoke_test.httpx, "get", lambda *_args, **_kwargs: DummyResponse(status_code=500, text="bad"))
    monkeypatch.setattr(smoke_test.sys, "argv", ["smoke_test.py"])
    assert smoke_test.main() == 1
    out = capsys.readouterr().out
    assert "failed" in out
    assert "bad" in out


def test_smoke_exception(monkeypatch, capsys):
    def boom(*_args, **_kwargs):
        raise RuntimeError("down")

    monkeypatch.setattr(smoke_test.httpx, "get", boom)
    monkeypatch.setattr(smoke_test.sys, "argv", ["smoke_test.py"])
    assert smoke_test.main() == 1
    assert "Gateway not responding" in capsys.readouterr().out


class DummyWebSocket:
    def __init__(self):
        self.sent = []
        self._messages = ["hello"]

    async def send(self, payload):
        self.sent.append(payload)

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self._messages:
            return self._messages.pop(0)
        raise StopAsyncIteration


class DummyConnect:
    def __init__(self, ws):
        self.ws = ws

    async def __aenter__(self):
        return self.ws

    async def __aexit__(self, exc_type, exc, tb):
        return False


def test_demo_build_headers(monkeypatch):
    monkeypatch.delenv("EUXIS_GATEWAY_TOKEN", raising=False)
    assert demo.build_headers() == {}
    monkeypatch.setenv("EUXIS_GATEWAY_TOKEN", "token")
    assert demo.build_headers() == {"Authorization": "Bearer token"}


def test_demo_main(monkeypatch):
    ws = DummyWebSocket()

    def fake_connect(*_args, **_kwargs):
        return DummyConnect(ws)

    monkeypatch.setattr(demo.websockets, "connect", fake_connect)
    monkeypatch.setattr(sys, "argv", ["demo.py"])

    result = asyncio.run(demo.main())
    assert result == 0
    assert len(ws.sent) == 2

def test_demo_main_guard(monkeypatch):
    import runpy

    def fake_run(coro):
        if hasattr(coro, "close"):
            coro.close()
        return 0

    sys.modules.pop("gateway.demo", None)
    monkeypatch.setattr(asyncio, "run", fake_run)
    monkeypatch.setattr(sys, "argv", ["demo.py"])
    with pytest.raises(SystemExit):
        runpy.run_module("gateway.demo", run_name="__main__")
