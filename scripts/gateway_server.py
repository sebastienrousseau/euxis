#!/usr/bin/env python3
"""Minimal Gateway server skeleton (WS + HTTP health)."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from gateway_adapter_registry import build_adapters
from gateway_utils import (
    load_session_from_disk,
    load_session_meta,
    load_run_events,
    persist_message,
    persist_run_event,
    persist_session_meta,
    runs_dir,
    timestamp,
)
DEFAULT_CONFIG = {
    "gateway": {
        "bind": "127.0.0.1",
        "port": 18789,
        "health_enabled": True,
        "health_path": "/health",
        "auth": {"mode": "token", "token": {"value": ""}, "password": {"value": ""}},
        "exec": {
            "policy": "allowlist",
            "ask": "on-miss",
            "ask_fallback": "deny",
            "allowlist": [],
        },
    }
}


class GatewayState:
    def __init__(self) -> None:
        self.started_at = time.time()
        self.sessions_active = 0
        self.sessions: Dict[str, List[Dict[str, Any]]] = {}
        self.session_version = 0
        self.last_event_ts = ""
        self.running: Dict[str, asyncio.Task] = {}
        self.pending_approvals: Dict[str, Dict[str, Any]] = {}
        self.adapters: Dict[str, Any] = {}
        self.connections: Dict[str, WebSocket] = {}
        self.conn_seq: Dict[str, int] = {}


STATE = GatewayState()


def load_config(path: Path | None) -> Dict[str, Any]:
    config = json.loads(json.dumps(DEFAULT_CONFIG))
    if path and path.exists():
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:  # pragma: no cover - config parse error
            print(f"Failed to read config: {exc}", file=sys.stderr)
            return config
        if isinstance(data, dict):
            config = merge_dicts(config, data)
    return config


def merge_dicts(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            base[key] = merge_dicts(base[key], value)
        else:
            base[key] = value
    return base


def resolve_config(args: argparse.Namespace) -> Dict[str, Any]:
    config_path = None
    if args.config:
        config_path = Path(args.config).expanduser()
    elif "EUXIS_GATEWAY_CONFIG" in os_environ():
        config_path = Path(os_environ()["EUXIS_GATEWAY_CONFIG"]).expanduser()
    else:
        config_path = Path("~/.euxis/config/gateway.json").expanduser()

    config = load_config(config_path)

    if args.bind:
        config["gateway"]["bind"] = args.bind
    if args.port:
        config["gateway"]["port"] = args.port
    if args.auth_mode:
        config["gateway"]["auth"]["mode"] = args.auth_mode

    config["gateway"].setdefault("channels", {})
    return config


def os_environ() -> Dict[str, str]:
    return dict(**os.environ)


def build_app(config: Dict[str, Any]) -> FastAPI:
    app = FastAPI()
    STATE.adapters = build_adapters(config)

    health_path = config["gateway"]["health_path"]
    health_enabled = config["gateway"]["health_enabled"]

    @app.get(health_path)  # type: ignore[misc]
    async def health() -> JSONResponse:
        if not health_enabled:
            return JSONResponse({"status": "disabled"}, status_code=404)
        uptime_ms = int((time.time() - STATE.started_at) * 1000)
        return JSONResponse(
            {
                "status": "ok",
                "sessions_active": STATE.sessions_active,
                "last_event_ts": STATE.last_event_ts or None,
                "uptime_ms": uptime_ms,
            }
        )

    @app.get("/approvals")
    async def approvals() -> JSONResponse:
        return JSONResponse({"pending": list(STATE.pending_approvals.values())})

    @app.get("/sessions")
    async def sessions() -> JSONResponse:
        sessions = []
        for session_id, messages in STATE.sessions.items():
            meta = load_session_meta(session_id)
            sessions.append(
                {
                    "session_id": session_id,
                    "message_count": len(messages),
                    "meta": meta,
                }
            )
        return JSONResponse({"sessions": sessions})

    @app.get("/sessions/{session_id}")
    async def session_detail(session_id: str) -> JSONResponse:
        ensure_session(session_id)
        if not STATE.sessions[session_id]:
            STATE.sessions[session_id] = load_session_from_disk(session_id)
        meta = load_session_meta(session_id)
        return JSONResponse({"session_id": session_id, "meta": meta, "messages": STATE.sessions[session_id]})

    @app.get("/sessions/export")
    async def sessions_export() -> JSONResponse:
        data: Dict[str, Any] = {}
        for session_id in STATE.sessions.keys():
            if not STATE.sessions[session_id]:
                STATE.sessions[session_id] = load_session_from_disk(session_id)
            data[session_id] = STATE.sessions[session_id]
        return JSONResponse({"sessions": data})

    @app.post("/sessions/import")
    async def sessions_import(payload: Dict[str, Any]) -> JSONResponse:
        sessions = payload.get("sessions", {})
        if not isinstance(sessions, dict):
            return JSONResponse({"status": "invalid"}, status_code=400)
        count = 0
        for session_id, entries in sessions.items():
            if not isinstance(entries, list):
                continue
            ensure_session(session_id)
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                persist_message(session_id, entry)
                count += 1
        return JSONResponse({"status": "ok", "imported": count})

    @app.get("/runs")
    async def runs() -> JSONResponse:
        runs = [p.stem for p in runs_dir().glob("*.jsonl")]
        return JSONResponse({"runs": runs})

    @app.get("/runs/{run_id}")
    async def run_detail(run_id: str) -> JSONResponse:
        return JSONResponse({"run_id": run_id, "events": load_run_events(run_id)})

    @app.post("/approvals/{run_id}/approve")
    async def approve(run_id: str) -> JSONResponse:
        pending = STATE.pending_approvals.pop(run_id, None)
        if not pending:
            return JSONResponse({"status": "not_found"}, status_code=404)
        pending["approved"] = True
        STATE.pending_approvals[run_id] = pending
        # Dispatch immediately to all active connections.
        for conn_id, ws in list(STATE.connections.items()):
            seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
            task = asyncio.create_task(
                dispatch_agent(
                    ws,
                    seq_state,
                    pending["session_id"],
                    pending["run_id"],
                    pending.get("meta", {}),
                    pending.get("content", ""),
                    config,
                )
            )
            STATE.running[pending["run_id"]] = task
            STATE.conn_seq[conn_id] = seq_state["value"]
            break
        return JSONResponse({"status": "approved"})

    @app.post("/approvals/{run_id}/reject")
    async def reject(run_id: str) -> JSONResponse:
        pending = STATE.pending_approvals.pop(run_id, None)
        if not pending:
            return JSONResponse({"status": "not_found"}, status_code=404)
        return JSONResponse({"status": "rejected"})

    @app.post("/admin/exec")
    async def update_exec_policy(payload: Dict[str, Any]) -> JSONResponse:
        exec_cfg = config["gateway"].setdefault("exec", {})
        for key in ("policy", "ask", "ask_fallback", "allowlist"):
            if key in payload:
                exec_cfg[key] = payload[key]
        return JSONResponse({"status": "ok", "exec": exec_cfg})

    @app.websocket("/")
    async def websocket_endpoint(ws: WebSocket) -> None:
        if not is_authorized(ws, config):
            await ws.close(code=4401)
            return
        await ws.accept()
        conn_id = f"conn_{int(time.time() * 1000)}"
        STATE.connections[conn_id] = ws
        STATE.conn_seq[conn_id] = 0
        seq_state = {"value": 0}
        tick_task = asyncio.create_task(send_ticks(ws, seq_state))
        await send_presence(ws, seq_state)
        try:
            while True:
                message = await ws.receive_text()
                await handle_frame(ws, message, seq_state, config)
        except WebSocketDisconnect:
            tick_task.cancel()
            STATE.connections.pop(conn_id, None)
            STATE.conn_seq.pop(conn_id, None)
            return

    webchat_dir = Path(__file__).resolve().parent / "gateway_webchat"
    if webchat_dir.exists():
        app.mount("/webchat", StaticFiles(directory=str(webchat_dir), html=True), name="webchat")

    return app


def is_authorized(ws: WebSocket, config: Dict[str, Any]) -> bool:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            return True
        header = ws.headers.get("authorization") or ""
        if header.strip() == f"Bearer {token}":
            return True
        allow_query = config["gateway"]["auth"].get("token", {}).get("allow_query_param", False)
        if allow_query:
            query_token = ws.query_params.get("token", "")
            return query_token == token
        return False
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            return True
        header = ws.headers.get("authorization") or ""
        if not header.lower().startswith("basic "):
            return False
        try:
            import base64
            encoded = header.split(" ", 1)[1].strip()
            decoded = base64.b64decode(encoded).decode("utf-8")
        except Exception:
            return False
        # Expect "username:password" format; only password is checked in v0.1
        if ":" not in decoded:
            return False
        _user, pwd = decoded.split(":", 1)
        return pwd == password
    return True


def ensure_session(session_id: str) -> None:
    if session_id not in STATE.sessions:
        STATE.sessions[session_id] = []
        STATE.sessions_active = len(STATE.sessions)
        STATE.session_version += 1


def append_message(session_id: str, role: str, content: str) -> Dict[str, Any]:
    entry = {
        "message_id": f"msg_{int(time.time() * 1000)}",
        "role": role,
        "content": content,
        "timestamp": timestamp(),
    }
    STATE.sessions[session_id].append(entry)
    persist_message(session_id, entry)
    return entry


async def handle_frame(
    ws: WebSocket, raw: str, seq_state: Dict[str, int], config: Dict[str, Any]
) -> None:
    try:
        frame = json.loads(raw)
    except Exception:
        await ws.send_text(
            json.dumps(
                {
                    "type": "response",
                    "id": "unknown",
                    "ok": False,
                    "error": {"code": "INVALID_REQUEST", "message": "Invalid JSON"},
                }
            )
        )
        return

    if frame.get("type") != "request":
        await ws.send_text(
            json.dumps(
                {
                    "type": "response",
                    "id": frame.get("id", "unknown"),
                    "ok": False,
                    "error": {"code": "INVALID_REQUEST", "message": "Expected request frame"},
                }
            )
        )
        return

    req_id = frame.get("id", "unknown")
    method = frame.get("method", "")
    params = frame.get("params", {}) if isinstance(frame.get("params"), dict) else {}

    if method == "chat.history":
        session_id = params.get("session_id", "")
        if not session_id:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id")
            return
        ensure_session(session_id)
        if not STATE.sessions[session_id]:
            STATE.sessions[session_id] = load_session_from_disk(session_id)
        limit = int(params.get("limit", 200))
        messages = STATE.sessions[session_id][-limit:]
        await send_result(ws, req_id, {"messages": messages})
        return

    if method == "chat.send":
        session_id = params.get("session_id", "")
        role = params.get("role", "")
        content = params.get("content", "")
        meta = params.get("meta", {}) if isinstance(params.get("meta"), dict) else {}
        if not session_id or not role:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id or role")
            return
        ensure_session(session_id)
        entry = append_message(session_id, role, content)
        session_meta = load_session_meta(session_id)
        session_meta.update(
            {
                "session_id": session_id,
                "channel_id": params.get("channel_id", "webchat"),
                "scope": params.get("scope", "dm"),
                "owner": meta.get("agent", "architect"),
                "updated_at": timestamp(),
            }
        )
        persist_session_meta(session_id, session_meta)
        run_id = f"run_{int(time.time() * 1000)}"
        if not is_exec_allowed(meta, config):
            pending = {
                "run_id": run_id,
                "session_id": session_id,
                "agent_id": meta.get("agent", "architect"),
                "requested_at": timestamp(),
                "meta": meta,
                "content": content,
                "approved": False,
            }
            STATE.pending_approvals[run_id] = pending
            await send_error(ws, req_id, "APPROVAL_REQUIRED", "Execution approval required")
            return
        await send_result(ws, req_id, {"message_id": entry["message_id"], "session_id": session_id, "run_id": run_id})
        task = asyncio.create_task(
            dispatch_agent(ws, seq_state, session_id, run_id, meta, content, config)
        )
        STATE.running[run_id] = task
        return

    if method == "chat.inject":
        session_id = params.get("session_id", "")
        role = params.get("role", "")
        content = params.get("content", "")
        if not session_id or not role:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id or role")
            return
        ensure_session(session_id)
        entry = append_message(session_id, role, content)
        await send_result(ws, req_id, {"message_id": entry["message_id"], "session_id": session_id})
        return

    if method == "chat.abort":
        run_id = params.get("run_id", "")
        task = STATE.running.get(run_id)
        if task:
            task.cancel()
            STATE.running.pop(run_id, None)
        await send_result(ws, req_id, {"aborted": True})
        return

    await send_error(ws, req_id, "INVALID_REQUEST", "Unknown method")


async def dispatch_agent(
    ws: WebSocket,
    seq_state: Dict[str, int],
    session_id: str,
    run_id: str,
    meta: Dict[str, Any],
    content: str,
    config: Dict[str, Any],
) -> None:
    agent_id = meta.get("agent", "architect")
    provider = meta.get("provider")
    cmd = [str(Path.home() / ".euxis" / "bin" / "euxis"), agent_id, content]
    if provider:
        cmd.append(provider)

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
    except Exception as exc:
        await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "error", str(exc))
        return

    async def stream_stdout() -> None:
        assert proc.stdout
        async for line in proc.stdout:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    async def stream_stderr() -> None:
        assert proc.stderr
        async for line in proc.stderr:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    await asyncio.gather(stream_stdout(), stream_stderr())
    await proc.wait()
    status = "final" if proc.returncode == 0 else "error"
    await send_agent_event(ws, seq_state, run_id, session_id, agent_id, status, f"exit {proc.returncode}")
    STATE.running.pop(run_id, None)


async def send_ticks(ws: WebSocket, seq_state: Dict[str, int]) -> None:
    try:
        while True:
            await asyncio.sleep(30)
            seq_state["value"] += 1
            payload = {
                "type": "event",
                "event": "tick",
                "seq": seq_state["value"],
                "ts": timestamp(),
                "data": {"uptime_ms": int((time.time() - STATE.started_at) * 1000), "stateVersion": STATE.session_version},
            }
            STATE.last_event_ts = payload["ts"]
            await ws.send_text(json.dumps(payload))
    except asyncio.CancelledError:
        return


async def send_presence(ws: WebSocket, seq_state: Dict[str, int]) -> None:
    seq_state["value"] += 1
    payload = {
        "type": "event",
        "event": "presence",
        "seq": seq_state["value"],
        "ts": timestamp(),
        "data": {"stateVersion": STATE.session_version, "sessions_active": STATE.sessions_active},
    }
    STATE.last_event_ts = payload["ts"]
    await ws.send_text(json.dumps(payload))


async def send_agent_event(
    ws: WebSocket,
    seq_state: Dict[str, int],
    run_id: str,
    session_id: str,
    agent_id: str,
    status: str,
    content: str,
) -> None:
    seq_state["value"] += 1
    payload = {
        "type": "event",
        "event": "agent",
        "seq": seq_state["value"],
        "ts": timestamp(),
        "data": {
            "run_id": run_id,
            "session_id": session_id,
            "agent_id": agent_id,
            "status": status,
            "content": content,
        },
    }
    STATE.last_event_ts = payload["ts"]
    persist_run_event(run_id, payload)
    await ws.send_text(json.dumps(payload))


def is_exec_allowed(meta: Dict[str, Any], config: Dict[str, Any]) -> bool:
    exec_cfg = config["gateway"].get("exec", {})
    policy = exec_cfg.get("policy", "allowlist")
    ask = exec_cfg.get("ask", "on-miss")
    ask_fallback = exec_cfg.get("ask_fallback", "deny")
    allowlist = set(exec_cfg.get("allowlist", []))
    approved = bool(meta.get("approved"))
    agent_id = meta.get("agent", "")

    if policy == "deny":
        return False
    if policy == "full":
        return True

    allowed = agent_id in allowlist
    if allowed:
        return True

    if ask == "off":
        return ask_fallback == "full"
    if ask == "always" and not approved:
        return False
    if ask == "on-miss" and not approved:
        return False
    return approved or ask_fallback == "full"


async def send_result(ws: WebSocket, req_id: str, result: Dict[str, Any]) -> None:
    await ws.send_text(
        json.dumps(
            {
                "type": "response",
                "id": req_id,
                "ok": True,
                "result": result,
            }
        )
    )


async def send_error(ws: WebSocket, req_id: str, code: str, message: str) -> None:
    await ws.send_text(
        json.dumps(
            {
                "type": "response",
                "id": req_id,
                "ok": False,
                "error": {"code": code, "message": message},
            }
        )
    )


def cmd_run(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    app = build_app(config)
    uvicorn.run(
        app,
        host=config["gateway"]["bind"],
        port=int(config["gateway"]["port"]),
        log_level="info",
    )
    return 0


def cmd_config(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    print(json.dumps(config, indent=2))
    return 0


def cmd_status(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    bind = config["gateway"]["bind"]
    port = config["gateway"]["port"]
    health_path = config["gateway"]["health_path"]
    url = f"http://{bind}:{port}{health_path}"
    try:
        import httpx

        resp = httpx.get(url, timeout=2.0)
        print(resp.text)
        return 0 if resp.status_code == 200 else 1
    except Exception:
        print("Gateway not responding")
        return 1


def cmd_sessions(args: argparse.Namespace) -> int:
    config = resolve_config(args)
    bind = config["gateway"]["bind"]
    port = config["gateway"]["port"]
    url = f"http://{bind}:{port}/sessions"
    try:
        import httpx

        resp = httpx.get(url, timeout=2.0)
        print(resp.text)
        return 0 if resp.status_code == 200 else 1
    except Exception:
        print("Gateway not responding")
        return 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Euxis Gateway CLI")
    subparsers = parser.add_subparsers(dest="command")

    def add_common(sub):
        sub.add_argument("--bind", help="Bind address")
        sub.add_argument("--port", type=int, help="Bind port")
        sub.add_argument("--config", help="Gateway config path")
        sub.add_argument("--auth-mode", help="Override auth mode")

    run_p = subparsers.add_parser("run", help="Run gateway")
    add_common(run_p)

    status_p = subparsers.add_parser("status", help="Gateway status")
    add_common(status_p)

    config_p = subparsers.add_parser("config", help="Print config")
    add_common(config_p)

    sessions_p = subparsers.add_parser("sessions", help="List sessions")
    add_common(sessions_p)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "run":
        return cmd_run(args)
    if args.command == "status":
        return cmd_status(args)
    if args.command == "config":
        return cmd_config(args)
    if args.command == "sessions":
        return cmd_sessions(args)

    parser.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
