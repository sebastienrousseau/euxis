import asyncio
import json

import pytest
Request = pytest.importorskip("starlette.requests").Request

from gateway import server


def test_voice_upload_validation(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True, "max_upload_size": 10}
    app = server.build_app(config)

    upload = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/upload")

    class DummyFile:
        def __init__(self, filename, data):
            self.filename = filename
            self._data = data

        async def stream(self):
            yield self._data

    scope = {"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)

    # Invalid extension
    resp = asyncio.run(upload("sess", DummyFile("audio.txt", b"data"), req))
    assert resp.status_code == 400

    # Too large
    resp = asyncio.run(upload("sess", DummyFile("audio.wav", b"0" * 20), req))
    assert resp.status_code == 413

    # Content mismatch
    resp = asyncio.run(upload("sess", DummyFile("audio.wav", b"bad" * 3), req))
    assert resp.status_code == 400


def test_voice_upload_missing_request(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {"enabled": True}
    app = server.build_app(config)

    upload = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/upload")

    class DummyFile:
        def __init__(self, filename, data):
            self.filename = filename
            self._data = data

        async def stream(self):
            yield self._data

    resp = asyncio.run(upload("sess", DummyFile("audio.wav", b"RIFFxxxxWAVE1234"), None))
    assert resp.status_code == 401


def test_voice_tts_and_stt(monkeypatch):
    config = server.load_config(None)
    config["gateway"]["auth"]["mode"] = "none"
    config["gateway"]["voice"] = {
        "enabled": True,
        "tts": {"mode": "webhook", "webhook_url": "http://example.com"},
        "stt": {"mode": "command", "command": "echo hi"},
        "command_allowlist": ["echo"],
    }
    app = server.build_app(config)

    tts = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/tts")
    stt = next(route.endpoint for route in app.routes if getattr(route, "path", None) == "/voice/stt")

    monkeypatch.setattr(server, "post_json", lambda *_args, **_kwargs: asyncio.sleep(0))

    async def fake_run_voice(*_args, **_kwargs):
        return "hello"

    monkeypatch.setattr(server, "run_voice_command", fake_run_voice)
    monkeypatch.setattr(server, "persist_voice_text", lambda *_args, **_kwargs: None)

    scope = {"type": "http", "headers": [], "query_string": b"", "client": ("127.0.0.2", 1234)}
    req = Request(scope)

    resp = asyncio.run(tts({"session_id": "sess", "content": "hi"}, req))
    assert resp.status_code == 200

    resp = asyncio.run(stt({"session_id": "sess", "audio_path": "path"}, req))
    assert resp.status_code == 200
