from __future__ import annotations

import sys
import types
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parents[1] / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

try:
    import fastapi  # noqa: F401
except Exception:
    fastapi = types.ModuleType("fastapi")

    class JSONResponse:
        def __init__(self, content, status_code=200, headers=None):
            self.content = content
            self.status_code = status_code
            self.headers = headers or {}

        def json(self):
            return self.content

    class UploadFile:
        def __init__(self, filename="file", data=b""):
            self.filename = filename
            self._data = data

        async def stream(self):
            yield self._data

    class WebSocketDisconnect(Exception):
        pass

    class WebSocket:
        def __init__(self, headers=None, query_params=None):
            self.headers = headers or {}
            self.query_params = query_params or {}
            self.sent = []
            self.closed = False

        async def accept(self):
            return None

        async def receive_text(self):
            raise WebSocketDisconnect()

        async def send_text(self, text):
            self.sent.append(text)

        async def close(self, code=1000):
            self.closed = True

    class Request:
        def __init__(self, headers=None, query_params=None, client=None):
            self.headers = headers or {}
            self.query_params = query_params or {}
            self.client = client

    def File(_default=None):
        return _default

    class StaticFiles:
        def __init__(self, directory=None, html=False):
            self.directory = directory
            self.html = html

    class CORSMiddleware:
        def __init__(self, *args, **kwargs):
            pass

    class FastAPI:
        def __init__(self, *args, **kwargs):
            self.routes = []
            self.middlewares = []
            self.lifespan = kwargs.get("lifespan")

        def add_middleware(self, *args, **kwargs):
            self.middlewares.append((args, kwargs))

        def middleware(self, _type):
            def decorator(fn):
                return fn
            return decorator

        def _add_route(self, method, path, fn):
            self.routes.append((method, path, fn))
            return fn

        def get(self, path):
            def decorator(fn):
                return self._add_route("GET", path, fn)
            return decorator

        def post(self, path):
            def decorator(fn):
                return self._add_route("POST", path, fn)
            return decorator

        def delete(self, path):
            def decorator(fn):
                return self._add_route("DELETE", path, fn)
            return decorator

        def websocket(self, path):
            def decorator(fn):
                return self._add_route("WS", path, fn)
            return decorator

        def on_event(self, _event):
            def decorator(fn):
                return fn
            return decorator

        def mount(self, *_args, **_kwargs):
            return None

    fastapi.FastAPI = FastAPI
    fastapi.File = File
    fastapi.Request = Request
    fastapi.UploadFile = UploadFile
    fastapi.WebSocket = WebSocket
    fastapi.WebSocketDisconnect = WebSocketDisconnect
    sys.modules["fastapi"] = fastapi

    responses = types.ModuleType("fastapi.responses")
    responses.JSONResponse = JSONResponse
    sys.modules["fastapi.responses"] = responses

    staticfiles = types.ModuleType("fastapi.staticfiles")
    staticfiles.StaticFiles = StaticFiles
    sys.modules["fastapi.staticfiles"] = staticfiles

    middleware = types.ModuleType("fastapi.middleware")
    cors = types.ModuleType("fastapi.middleware.cors")
    cors.CORSMiddleware = CORSMiddleware
    middleware.cors = cors
    sys.modules["fastapi.middleware"] = middleware
    sys.modules["fastapi.middleware.cors"] = cors

    uvicorn = types.ModuleType("uvicorn")
    uvicorn.run = lambda *_args, **_kwargs: None
    sys.modules["uvicorn"] = uvicorn

def _install_gateway_lazy_loader(src_dir: Path) -> None:
    if "gateway" in sys.modules:
        return

    gateway_pkg = types.ModuleType("gateway")
    gateway_pkg.__path__ = [str(src_dir / "gateway")]

    class _LazyModule(types.ModuleType):
        def __init__(self, attr_name: str) -> None:
            super().__init__(f"gateway.{attr_name}")
            self._attr_name = attr_name
            self._loaded = None

        def _load(self):
            if self._loaded is None:
                import importlib
                self._loaded = importlib.import_module(f"gateway.{self._attr_name}")
            return self._loaded

        def __getattr__(self, name: str):
            return getattr(self._load(), name)

        def __setattr__(self, name: str, value) -> None:
            if name in {"_attr_name", "_loaded"}:
                object.__setattr__(self, name, value)
                return
            setattr(self._load(), name, value)

    _lazy_names = {
        "server",
        "protocol_test",
        "session_export",
        "session_import",
        "smoke_test",
        "demo",
    }

    def __getattr__(name: str):
        if name in _lazy_names:
            return _LazyModule(name)
        raise AttributeError(name)

    gateway_pkg.__getattr__ = __getattr__  # type: ignore[attr-defined]
    sys.modules["gateway"] = gateway_pkg


_install_gateway_lazy_loader(SRC_DIR)

import pytest
from gateway import server


@pytest.fixture(autouse=True)
def reset_gateway_state():
    server.STATE = server.GatewayState()
    server._rate_limit_state.clear()
    server._auth_failure_state.clear()
    yield
