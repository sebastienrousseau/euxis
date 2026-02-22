import json
from pathlib import Path

import pytest

from gateway import protocol_test


def test_minimal_validate_missing_fields():
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({"type": "request"})
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({"type": "response", "id": "r1"})
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({"type": "event", "event": "presence", "seq": 1, "ts": "x", "data": {}})


def test_validate_with_jsonschema_errors(monkeypatch, tmp_path: Path):
    pytest.importorskip("jsonschema")
    monkeypatch.setattr(protocol_test, "REQUEST_SCHEMA", tmp_path / "missing.json")
    monkeypatch.setattr(protocol_test, "RESPONSE_SCHEMA", tmp_path / "missing.json")
    monkeypatch.setattr(protocol_test, "EVENT_SCHEMA", tmp_path / "missing.json")
    monkeypatch.setattr(protocol_test, "FRAME_SCHEMA", tmp_path / "missing.json")

    def fake_read_text(*_args, **_kwargs):
        return json.dumps({"type": "object"})

    monkeypatch.setattr(Path, "read_text", fake_read_text)
    errors = protocol_test.validate_with_jsonschema([{ "type": "request" }])
    assert errors == []


def test_main_schema_validation_pass(monkeypatch, capsys):
    monkeypatch.setattr(protocol_test, "load_frames", lambda _: protocol_test.SAMPLE_FRAMES)
    monkeypatch.setattr(protocol_test, "validate_with_jsonschema", lambda frames: [])
    monkeypatch.setattr(protocol_test.sys, "argv", ["protocol_test.py"])
    result = protocol_test.main()
    assert result == 0
    assert "schema validation passed" in capsys.readouterr().out


def test_main_schema_validation_fail(monkeypatch, capsys):
    monkeypatch.setattr(protocol_test, "load_frames", lambda _: protocol_test.SAMPLE_FRAMES)
    monkeypatch.setattr(protocol_test, "validate_with_jsonschema", lambda frames: ["bad"]) 
    monkeypatch.setattr(protocol_test.sys, "argv", ["protocol_test.py"])
    result = protocol_test.main()
    assert result == 1
    assert "validation failed" in capsys.readouterr().out

def test_minimal_validate_additional_branches():
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "chat.history",
            "params": {},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "chat.abort",
            "params": {"session_id": "s"},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "chat.inject",
            "params": {"session_id": "s"},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "response",
            "id": "r1",
            "ok": True,
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "event",
            "event": "shutdown",
            "seq": 1,
            "ts": "x",
            "data": {"reason": "x"},
        })


def test_minimal_validate_missing_keys_and_invalids():
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({"type": "bad"})
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "gateway.connect",
            "params": {},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "request",
            "id": "r1",
            "method": "chat.send",
            "params": {"session_id": "s"},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "response",
            "id": "r1",
            "ok": False,
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "event",
            "event": "agent",
            "seq": 1,
            "ts": "x",
            "data": {"run_id": "r1"},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "event",
            "event": "tick",
            "seq": 1,
            "ts": "x",
            "data": {"uptime_ms": 5},
        })
    with pytest.raises(protocol_test.ValidationError):
        protocol_test.minimal_validate({
            "type": "event",
            "event": "mystery",
            "seq": 1,
            "ts": "x",
            "data": {},
        })


def test_validate_with_jsonschema_refresolver(monkeypatch):
    import builtins

    original_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name.startswith("referencing"):
            raise ImportError("no referencing")
        return original_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)

    schema = {"$id": "schema", "type": "object"}
    tmp = Path("/tmp/pt_schema.json")
    tmp.write_text(json.dumps(schema), encoding="utf-8")
    monkeypatch.setattr(protocol_test, "REQUEST_SCHEMA", tmp)
    monkeypatch.setattr(protocol_test, "RESPONSE_SCHEMA", tmp)
    monkeypatch.setattr(protocol_test, "EVENT_SCHEMA", tmp)
    monkeypatch.setattr(protocol_test, "FRAME_SCHEMA", tmp)

    errors = protocol_test.validate_with_jsonschema(protocol_test.SAMPLE_FRAMES)
    assert errors == ["referencing not installed; using minimal validation"]


def test_validate_with_jsonschema_collects_errors(monkeypatch, tmp_path):
    pytest.importorskip("jsonschema")
    schema = {"$id": "schema", "type": "object", "required": ["type"]}
    path = tmp_path / "schema.json"
    path.write_text(json.dumps(schema), encoding="utf-8")
    monkeypatch.setattr(protocol_test, "REQUEST_SCHEMA", path)
    monkeypatch.setattr(protocol_test, "RESPONSE_SCHEMA", path)
    monkeypatch.setattr(protocol_test, "EVENT_SCHEMA", path)
    monkeypatch.setattr(protocol_test, "FRAME_SCHEMA", path)
    errors = protocol_test.validate_with_jsonschema([{}])
    assert errors and errors[0].startswith("frame 1:")


def test_load_frames_skips_blank_lines(tmp_path: Path):
    payload = "\n".join([
        json.dumps({"type": "request", "id": "r1", "method": "gateway.connect", "params": {"protocol": "v0.1"}}),
        "",
        json.dumps({"type": "response", "id": "r1", "ok": True, "result": {"ok": True}}),
    ])
    path = tmp_path / "frames.jsonl"
    path.write_text(payload, encoding="utf-8")
    frames = protocol_test.load_frames(path)
    assert len(frames) == 2
