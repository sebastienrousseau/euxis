import builtins
import json
from pathlib import Path

import pytest

from gateway import protocol_test


def test_load_frames_default():
    frames = protocol_test.load_frames(None)
    assert frames
    assert frames[0]["type"] == "request"


def test_load_frames_json_array(tmp_path: Path):
    payload = [{"type": "request", "id": "r1", "method": "gateway.connect", "params": {"protocol": "v0.1"}}]
    path = tmp_path / "frames.json"
    path.write_text(json.dumps(payload), encoding="utf-8")
    frames = protocol_test.load_frames(path)
    assert frames == payload


def test_load_frames_jsonl(tmp_path: Path):
    path = tmp_path / "frames.jsonl"
    path.write_text("\n".join([
        json.dumps({"type": "response", "id": "r1", "ok": True, "result": {"ok": True}}),
        "",
    ]), encoding="utf-8")
    frames = protocol_test.load_frames(path)
    assert frames[0]["type"] == "response"


def test_minimal_validate_request_and_response():
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r1",
        "method": "chat.send",
        "params": {"session_id": "s1", "channel_id": "webchat", "role": "user", "content": "hi"},
    })
    protocol_test.minimal_validate({
        "type": "response",
        "id": "r1",
        "ok": False,
        "error": {"message": "nope"},
    })


def test_minimal_validate_event():
    protocol_test.minimal_validate({
        "type": "event",
        "event": "tick",
        "seq": 1,
        "ts": "2026-02-21T00:00:00Z",
        "data": {"uptime_ms": 10, "stateVersion": 1},
    })


def test_minimal_validate_invalid_method():
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "unknown",
            "params": {},
        })


def test_validate_with_jsonschema_fallback(monkeypatch):
    original_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name == "jsonschema":
            raise ImportError("nope")
        return original_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)
    errors = protocol_test.validate_with_jsonschema(protocol_test.SAMPLE_FRAMES)
    assert errors and errors[0].startswith("jsonschema not installed")


def test_main_empty_frames(tmp_path: Path, monkeypatch, capsys):
    empty = tmp_path / "empty.jsonl"
    empty.write_text("", encoding="utf-8")
    monkeypatch.setattr(protocol_test, "load_frames", lambda _: [])
    monkeypatch.setattr(protocol_test.sys, "argv", ["protocol_test.py", "--frames", str(empty)])
    result = protocol_test.main()
    assert result == 1
    assert "no frames found" in capsys.readouterr().out


def test_main_minimal_validation_failure(monkeypatch, capsys):
    monkeypatch.setattr(protocol_test, "load_frames", lambda _: [{"type": "event"}])
    monkeypatch.setattr(protocol_test, "validate_with_jsonschema", lambda _: ["jsonschema not installed; using minimal validation"])
    monkeypatch.setattr(protocol_test.sys, "argv", ["protocol_test.py"])
    result = protocol_test.main()
    assert result == 1
    assert "validation failed" in capsys.readouterr().out
