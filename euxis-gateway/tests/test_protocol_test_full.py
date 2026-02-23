import json
from pathlib import Path

import pytest

from gateway import protocol_test


def test_load_frames_empty_file(tmp_path: Path):
    path = tmp_path / "empty.jsonl"
    path.write_text("\n", encoding="utf-8")
    assert protocol_test.load_frames(path) == []


def test_minimal_validate_event_branches():
    protocol_test.minimal_validate({
        "type": "event",
        "event": "agent",
        "seq": 1,
        "ts": "x",
        "data": {"run_id": "r", "session_id": "s", "agent_id": "a", "status": "final", "content": "ok"},
    })
    protocol_test.minimal_validate({
        "type": "event",
        "event": "presence",
        "seq": 1,
        "ts": "x",
        "data": {"stateVersion": 1, "sessions_active": 0},
    })
    protocol_test.minimal_validate({
        "type": "event",
        "event": "tick",
        "seq": 1,
        "ts": "x",
        "data": {"uptime_ms": 10, "stateVersion": 1},
    })
    protocol_test.minimal_validate({
        "type": "event",
        "event": "shutdown",
        "seq": 1,
        "ts": "x",
        "data": {"reason": "done", "message": "bye"},
    })


def test_validate_with_jsonschema_success(monkeypatch, tmp_path: Path):
    schema = {"$id": "schema", "type": "object"}
    for path in [
        tmp_path / "req.json",
        tmp_path / "res.json",
        tmp_path / "evt.json",
        tmp_path / "frame.json",
    ]:
        path.write_text(json.dumps(schema), encoding="utf-8")

    monkeypatch.setattr(protocol_test, "REQUEST_SCHEMA", tmp_path / "req.json")
    monkeypatch.setattr(protocol_test, "RESPONSE_SCHEMA", tmp_path / "res.json")
    monkeypatch.setattr(protocol_test, "EVENT_SCHEMA", tmp_path / "evt.json")
    monkeypatch.setattr(protocol_test, "FRAME_SCHEMA", tmp_path / "frame.json")

    errors = protocol_test.validate_with_jsonschema([{ "type": "request" }])
    assert errors == []


def test_main_minimal_validation_success(monkeypatch, capsys):
    monkeypatch.setattr(protocol_test, "load_frames", lambda _: [{
        "type": "request",
        "id": "r1",
        "method": "gateway.connect",
        "params": {"protocol": "v0.1"},
    }])
    monkeypatch.setattr(protocol_test, "validate_with_jsonschema", lambda frames: ["jsonschema not installed; using minimal validation"])
    monkeypatch.setattr(protocol_test.sys, "argv", ["protocol_test.py"])
    result = protocol_test.main()
    assert result == 0
    out = capsys.readouterr().out
    assert "minimal validation passed" in out

def test_minimal_validate_request_variants():
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r1",
        "method": "gateway.connect",
        "params": {"protocol": "v0.1"},
    })
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r2",
        "method": "chat.history",
        "params": {"session_id": "s"},
    })
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r3",
        "method": "chat.send",
        "params": {"session_id": "s", "channel_id": "c", "role": "user", "content": "hi"},
    })
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r4",
        "method": "chat.abort",
        "params": {"run_id": "r", "session_id": "s"},
    })
    protocol_test.minimal_validate({
        "type": "request",
        "id": "r5",
        "method": "chat.inject",
        "params": {"session_id": "s", "role": "user", "content": "hi"},
    })
