# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

#!/usr/bin/env python3
"""Minimal Gateway protocol test harness.

Validates sample frames against JSON Schemas when jsonschema is available.
Falls back to lightweight validation if jsonschema is not installed.
Uses referencing-based resolution when available to avoid deprecated resolvers.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCHEMA_DIR = ROOT / "docs" / "reference" / "gateway"

REQUEST_SCHEMA = SCHEMA_DIR / "gateway-request.schema.json"
RESPONSE_SCHEMA = SCHEMA_DIR / "gateway-response.schema.json"
EVENT_SCHEMA = SCHEMA_DIR / "gateway-event.schema.json"
FRAME_SCHEMA = SCHEMA_DIR / "gateway-frame.schema.json"

SAMPLE_FRAMES = [
    {
        "type": "request",
        "id": "req_connect_1",
        "method": "gateway.connect",
        "params": {"protocol": "v0.1", "client_id": "demo"},
    },
    {
        "type": "request",
        "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
        "method": "chat.send",
        "params": {
            "session_id": "sess_webchat_abc123",
            "channel_id": "webchat",
            "role": "user",
            "content": "Design a caching layer",
            "meta": {"agent": "architect", "priority": "P1", "stream": True},
        },
    },
    {
        "type": "response",
        "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
        "ok": True,
        "result": {
            "message_id": "msg_01HZX0M6BQ9Z3P7J8R2H8J6KCN",
            "session_id": "sess_webchat_abc123",
        },
    },
    {
        "type": "event",
        "event": "agent",
        "seq": 81,
        "ts": "2026-02-15T19:45:12.118Z",
        "data": {
            "run_id": "run_01HZX0NAN9ZK4D8FMQ8J8C1K0E",
            "session_id": "sess_webchat_abc123",
            "agent_id": "architect",
            "status": "stream",
            "content": "Design outline: ...",
        },
    },
]


class ValidationError(Exception):
    pass


def load_frames(path: Path | None) -> list[dict]:
    if path is None:
        return SAMPLE_FRAMES
    raw = path.read_text(encoding="utf-8").strip()
    if not raw:
        return []
    if raw.startswith("["):
        return json.loads(raw)
    frames = []
    for line in raw.splitlines():
        if not line.strip():
            continue
        frames.append(json.loads(line))
    return frames


def minimal_validate(frame: dict) -> None:
    frame_type = frame.get("type")
    if frame_type not in {"request", "response", "event"}:
        raise ValidationError("type must be request|response|event")

    if frame_type == "request":
        for key in ("id", "method", "params"):
            if key not in frame:
                raise ValidationError(f"missing {key}")
        method = frame["method"]
        params = frame["params"]
        if method == "gateway.connect":
            if "protocol" not in params:
                raise ValidationError("gateway.connect requires protocol")
        elif method == "chat.history":
            if "session_id" not in params:
                raise ValidationError("chat.history requires session_id")
        elif method == "chat.send":
            for key in ("session_id", "channel_id", "role", "content"):
                if key not in params:
                    raise ValidationError(f"chat.send requires {key}")
        elif method == "chat.abort":
            for key in ("run_id", "session_id"):
                if key not in params:
                    raise ValidationError(f"chat.abort requires {key}")
        elif method == "chat.inject":
            for key in ("session_id", "role", "content"):
                if key not in params:
                    raise ValidationError(f"chat.inject requires {key}")
        else:
            raise ValidationError("invalid method")

    if frame_type == "response":
        if "id" not in frame or "ok" not in frame:
            raise ValidationError("response requires id and ok")
        if frame["ok"] is True and "result" not in frame:
            raise ValidationError("ok responses require result")
        if frame["ok"] is False and "error" not in frame:
            raise ValidationError("error responses require error")

    if frame_type == "event":
        for key in ("event", "seq", "ts", "data"):
            if key not in frame:
                raise ValidationError(f"event requires {key}")
        event_name = frame["event"]
        data = frame["data"]
        if event_name == "agent":
            for key in ("run_id", "session_id", "agent_id", "status", "content"):
                if key not in data:
                    raise ValidationError(f"agent event requires {key}")
        elif event_name == "presence":
            for key in ("stateVersion", "sessions_active"):
                if key not in data:
                    raise ValidationError(f"presence event requires {key}")
        elif event_name == "tick":
            for key in ("uptime_ms", "stateVersion"):
                if key not in data:
                    raise ValidationError(f"tick event requires {key}")
        elif event_name == "shutdown":
            for key in ("reason", "message"):
                if key not in data:
                    raise ValidationError(f"shutdown event requires {key}")
        else:
            raise ValidationError("invalid event")


def validate_with_jsonschema(frames: list[dict]) -> list[str]:
    try:
        import jsonschema
    except Exception:
        return ["jsonschema not installed; using minimal validation"]

    request_schema = json.loads(REQUEST_SCHEMA.read_text(encoding="utf-8"))
    response_schema = json.loads(RESPONSE_SCHEMA.read_text(encoding="utf-8"))
    event_schema = json.loads(EVENT_SCHEMA.read_text(encoding="utf-8"))
    frame_schema = json.loads(FRAME_SCHEMA.read_text(encoding="utf-8"))

    # Map schema IDs and file URIs to local content to avoid remote fetches.
    store = {
        request_schema.get("$id"): request_schema,
        response_schema.get("$id"): response_schema,
        event_schema.get("$id"): event_schema,
        frame_schema.get("$id"): frame_schema,
        REQUEST_SCHEMA.as_uri(): request_schema,
        RESPONSE_SCHEMA.as_uri(): response_schema,
        EVENT_SCHEMA.as_uri(): event_schema,
        FRAME_SCHEMA.as_uri(): frame_schema,
    }

    try:
        from referencing import Registry, Resource
        from referencing.jsonschema import DRAFT7

        registry = Registry()
        for key, schema in store.items():
            if not key:
                continue
            registry = registry.with_resource(
                key, Resource.from_contents(schema, default_specification=DRAFT7)
            )
        validator = jsonschema.Draft7Validator(frame_schema, registry=registry)
    except Exception:
        resolver = jsonschema.RefResolver.from_schema(frame_schema, store=store)
        validator = jsonschema.Draft7Validator(frame_schema, resolver=resolver)

    errors = []
    for idx, frame in enumerate(frames, start=1):
        for err in validator.iter_errors(frame):
            errors.append(f"frame {idx}: {err.message}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate Euxis Gateway frames")
    parser.add_argument("--frames", type=Path, help="JSON array or JSONL of frames")
    args = parser.parse_args()

    frames = load_frames(args.frames)
    if not frames:
        print("no frames found")
        return 1

    errors = validate_with_jsonschema(frames)
    if errors and errors[0].startswith("jsonschema not installed"):
        try:
            for frame in frames:
                minimal_validate(frame)
        except ValidationError as exc:
            print(f"validation failed: {exc}")
            return 1
        print(errors[0])
        print("minimal validation passed")
        return 0

    if errors:
        print("validation failed:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("schema validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
