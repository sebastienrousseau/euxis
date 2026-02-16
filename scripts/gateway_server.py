#!/usr/bin/env python3
"""Minimal Gateway server skeleton (WS + HTTP health)."""

from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import hmac
import json
import os
import sys
import time
from urllib import error as urlerror
from urllib import request as urlrequest
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from fastapi import FastAPI, File, Request, UploadFile, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

from gateway_adapter_registry import build_adapters
from gateway_utils import (
    load_session_from_disk,
    load_session_meta,
    load_run_events,
    load_approvals,
    load_cron_jobs,
    load_canvas_state,
    load_webhooks,
    persist_approval,
    persist_cron_jobs,
    persist_webhooks,
    delete_approval,
    audit_log,
    persist_transcript,
    persist_message,
    persist_run_event,
    persist_session_meta,
    persist_canvas_state,
    persist_voice_blob,
    persist_voice_text,
    append_voice_chunk,
    runs_dir,
    make_session_id,
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
        "channels": {
            "slack": {"enabled": False, "mode": "socket", "token": "", "app_token": "", "signing_secret": ""},
            "telegram": {"enabled": False, "mode": "webhook", "token": "", "webhook_url": ""},
        },
        "policy": {"mention_required": True, "allowlist": []},
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
        self.conn_sessions: Dict[str, str] = {}
        self.session_locks: Dict[str, asyncio.Lock] = {}
        self.cron_jobs: List[Dict[str, Any]] = []
        self.cron_task: Optional[asyncio.Task] = None
        self.config: Dict[str, Any] = {}
        self.webhooks: List[Dict[str, Any]] = []


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


def verify_slack_signature(request: Request, body: bytes, signing_secret: str) -> bool:
    timestamp = request.headers.get("X-Slack-Request-Timestamp", "")
    signature = request.headers.get("X-Slack-Signature", "")
    if not timestamp or not signature:
        return False
    try:
        ts_value = int(timestamp)
    except ValueError:
        return False
    if abs(time.time() - ts_value) > 300:
        return False
    base = f"v0:{timestamp}:".encode("utf-8") + body
    digest = hmac.new(signing_secret.encode("utf-8"), base, hashlib.sha256).hexdigest()
    expected = f"v0={digest}"
    return hmac.compare_digest(expected, signature)


def build_app(config: Dict[str, Any]) -> FastAPI:
    app = FastAPI()
    STATE.config = config
    session_cfg = config["gateway"].setdefault("session", {})
    session_cfg.setdefault("dm_scope", "main")
    session_cfg.setdefault("account_id", "default")

    def _adapter_handler(session_id: str, content: str, meta: Dict[str, Any]) -> None:
        channel_id = meta.get("channel_id", "unknown")
        chat_id = str(meta.get("chat_id") or meta.get("channel_id") or "unknown")
        thread_id = meta.get("thread_id") or meta.get("thread_ts")
        sender = meta.get("sender")
        resolved_id = make_session_id(
            channel_id,
            chat_id,
            str(thread_id) if thread_id else None,
            session_cfg.get("dm_scope", "main"),
            session_cfg.get("account_id", "default"),
            str(sender) if sender else None,
        )
        normalized_meta = dict(meta)
        normalized_meta.setdefault("channel_id", channel_id)
        normalized_meta.setdefault("scope", "dm")
        normalized_meta.setdefault("agent", "architect")
        normalized_meta.setdefault("approved", True)
        asyncio.create_task(process_inbound(resolved_id, content, normalized_meta, config))

    STATE.adapters = build_adapters(config, on_message=_adapter_handler)
    STATE.pending_approvals = load_approvals()
    slack_cfg = config["gateway"].get("channels", {}).get("slack", {})
    telegram_cfg = config["gateway"].get("channels", {}).get("telegram", {})

    health_path = config["gateway"]["health_path"]
    health_enabled = config["gateway"]["health_enabled"]

    @app.on_event("startup")
    async def startup() -> None:
        STATE.cron_jobs = load_cron_jobs()
        persisted_hooks = load_webhooks()
        if persisted_hooks:
            STATE.webhooks = persisted_hooks
        else:
            STATE.webhooks = config["gateway"].get("webhooks", [])
        STATE.cron_task = asyncio.create_task(cron_loop(config))
        for name, adapter in STATE.adapters.items():
            try:
                adapter.connect()
            except Exception as exc:  # pragma: no cover - best effort
                print(f"Adapter {name} connect failed: {exc}", file=sys.stderr)

    @app.on_event("shutdown")
    async def shutdown() -> None:
        if STATE.cron_task:
            STATE.cron_task.cancel()
        for name, adapter in STATE.adapters.items():
            try:
                adapter.disconnect()
            except Exception as exc:  # pragma: no cover - best effort
                print(f"Adapter {name} disconnect failed: {exc}", file=sys.stderr)

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

    @app.get("/automation/cron")
    async def cron_list() -> JSONResponse:
        return JSONResponse({"jobs": STATE.cron_jobs})

    @app.post("/automation/cron")
    async def cron_create(payload: Dict[str, Any]) -> JSONResponse:
        job_id = payload.get("job_id") or f"cron_{int(time.time() * 1000)}"
        every = payload.get("every_seconds")
        session_id = payload.get("session_id", "")
        content = payload.get("content", "")
        meta = payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}
        if not every or not session_id or not content:
            return JSONResponse({"status": "invalid"}, status_code=400)
        job = {
            "job_id": job_id,
            "every_seconds": int(every),
            "session_id": session_id,
            "content": content,
            "meta": meta,
            "last_run": 0,
        }
        STATE.cron_jobs = [j for j in STATE.cron_jobs if j.get("job_id") != job_id] + [job]
        persist_cron_jobs(STATE.cron_jobs)
        return JSONResponse({"status": "ok", "job": job})

    @app.delete("/automation/cron/{job_id}")
    async def cron_delete(job_id: str) -> JSONResponse:
        before = len(STATE.cron_jobs)
        STATE.cron_jobs = [job for job in STATE.cron_jobs if job.get("job_id") != job_id]
        persist_cron_jobs(STATE.cron_jobs)
        if len(STATE.cron_jobs) == before:
            return JSONResponse({"status": "not_found"}, status_code=404)
        return JSONResponse({"status": "deleted"})

    @app.get("/webhooks")
    async def webhooks_list() -> JSONResponse:
        return JSONResponse({"webhooks": STATE.webhooks})

    @app.post("/webhooks")
    async def webhooks_set(payload: Dict[str, Any]) -> JSONResponse:
        hooks = payload.get("webhooks", [])
        if not isinstance(hooks, list):
            return JSONResponse({"status": "invalid"}, status_code=400)
        STATE.webhooks = hooks
        persist_webhooks(hooks)
        return JSONResponse({"status": "ok", "webhooks": hooks})

    slack_events_path = slack_cfg.get("events_path", "/channels/slack/events")
    telegram_webhook_path = telegram_cfg.get("webhook_path", "/channels/telegram/webhook")

    @app.post(slack_events_path)
    async def slack_events(request: Request) -> JSONResponse:
        adapter = STATE.adapters.get("slack")
        if not adapter:
            return JSONResponse({"status": "disabled"}, status_code=404)
        body = await request.body()
        try:
            payload = json.loads(body.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            return JSONResponse({"status": "invalid"}, status_code=400)

        if payload.get("type") == "url_verification":
            return JSONResponse({"challenge": payload.get("challenge", "")})

        signing_secret = slack_cfg.get("signing_secret", "")
        if signing_secret and not verify_slack_signature(request, body, signing_secret):
            return JSONResponse({"status": "unauthorized"}, status_code=401)

        adapter.receive(payload)
        return JSONResponse({"status": "ok"})

    @app.post(telegram_webhook_path)
    async def telegram_webhook(payload: Dict[str, Any]) -> JSONResponse:
        adapter = STATE.adapters.get("telegram")
        if not adapter:
            return JSONResponse({"status": "disabled"}, status_code=404)
        adapter.receive(payload)
        return JSONResponse({"status": "ok"})

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

    @app.get("/canvas/{session_id}")
    async def canvas_get(session_id: str) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        return JSONResponse({"session_id": session_id, "state": load_canvas_state(session_id)})

    @app.post("/canvas/{session_id}")
    async def canvas_set(session_id: str, payload: Dict[str, Any]) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        state = payload.get("state", {})
        if not isinstance(state, dict):
            return JSONResponse({"status": "invalid"}, status_code=400)
        persist_canvas_state(session_id, state)
        return JSONResponse({"status": "ok"})

    @app.post("/canvas/{session_id}/validate")
    async def canvas_validate(session_id: str, payload: Dict[str, Any]) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        state = payload.get("state", {})
        if not isinstance(state, dict):
            return JSONResponse({"status": "invalid"}, status_code=400)
        errors = validate_canvas(state)
        return JSONResponse({"status": "ok" if not errors else "invalid", "errors": errors})

    @app.post("/voice/wake")
    async def voice_wake(payload: Dict[str, Any]) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        session_id = payload.get("session_id", "")
        content = payload.get("content", "")
        meta = payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}
        if not session_id or not content:
            return JSONResponse({"status": "invalid"}, status_code=400)
        meta.setdefault("agent", "orchestrator")
        meta.setdefault("channel_id", "voice")
        meta.setdefault("scope", "dm")
        meta.setdefault("approved", True)
        persist_voice_text(
            session_id,
            {"ts": timestamp(), "event": "wake", "content": content, "meta": meta},
        )
        asyncio.create_task(process_inbound(session_id, content, meta, config))
        return JSONResponse({"status": "ok"})

    @app.post("/voice/talk")
    async def voice_talk(payload: Dict[str, Any]) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        session_id = payload.get("session_id", "")
        content = payload.get("content", "")
        meta = payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}
        if not session_id or not content:
            return JSONResponse({"status": "invalid"}, status_code=400)
        meta.setdefault("agent", "orchestrator")
        meta.setdefault("channel_id", "voice")
        meta.setdefault("scope", "dm")
        meta.setdefault("approved", True)
        persist_voice_text(
            session_id,
            {"ts": timestamp(), "event": "talk", "content": content, "meta": meta},
        )
        asyncio.create_task(process_inbound(session_id, content, meta, config))
        return JSONResponse({"status": "ok"})

    @app.post("/voice/upload")
    async def voice_upload(session_id: str, file: UploadFile = File(...)) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        if not session_id:
            return JSONResponse({"status": "invalid"}, status_code=400)
        blob = await file.read()
        suffix = (file.filename or "audio").split(".")[-1]
        path = persist_voice_blob(session_id, blob, suffix)
        return JSONResponse({"status": "ok", "path": str(path)})

    @app.post("/sessions/{session_id}/broadcast")
    async def session_broadcast(session_id: str, payload: Dict[str, Any]) -> JSONResponse:
        message = payload.get("message", "")
        if not message:
            return JSONResponse({"status": "invalid"}, status_code=400)
        for conn_id, ws in list(STATE.connections.items()):
            if STATE.conn_sessions.get(conn_id) != session_id:
                continue
            seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
            await send_agent_event(ws, seq_state, f"run_broadcast_{int(time.time() * 1000)}", session_id, "gateway", "final", message)
            STATE.conn_seq[conn_id] = seq_state["value"]
        return JSONResponse({"status": "ok"})

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
        persist_approval(run_id, pending)
        persist_transcript(
            run_id,
            {"ts": timestamp(), "event": "approval", "status": "approved", "entry": pending},
        )
        audit_log({"ts": timestamp(), "event": "approval", "run_id": run_id, "status": "approved"})
        # Dispatch immediately to all active connections.
        for conn_id, ws in list(STATE.connections.items()):
            seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
            task = asyncio.create_task(
                dispatch_with_lock(
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
        delete_approval(run_id)
        persist_transcript(
            run_id,
            {"ts": timestamp(), "event": "approval", "status": "rejected", "entry": pending},
        )
        audit_log({"ts": timestamp(), "event": "approval", "run_id": run_id, "status": "rejected"})
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
        allowed, reason = is_authorized(ws, config)
        if not allowed:
            await ws.accept()
            await ws.send_text(
                json.dumps(
                    {
                        "type": "response",
                        "id": "auth",
                        "ok": False,
                        "error": {"code": "UNAUTHORIZED", "message": reason or "unauthorized"},
                    }
                )
            )
            await ws.close(code=4401)
            return
        await ws.accept()
        conn_id = f"conn_{int(time.time() * 1000)}"
        STATE.connections[conn_id] = ws
        STATE.conn_seq[conn_id] = 0
        STATE.conn_sessions[conn_id] = ""
        seq_state = {"value": 0}
        tick_task = asyncio.create_task(send_ticks(ws, seq_state))
        await send_presence(ws, seq_state)
        try:
            while True:
                message = await ws.receive_text()
                await handle_frame(ws, message, seq_state, config, conn_id)
        except WebSocketDisconnect:
            tick_task.cancel()
            STATE.connections.pop(conn_id, None)
            STATE.conn_seq.pop(conn_id, None)
            STATE.conn_sessions.pop(conn_id, None)
            return

    @app.websocket("/voice/stream")
    async def voice_stream(ws: WebSocket) -> None:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            await ws.accept()
            await ws.send_text(json.dumps({"type": "error", "error": "voice_disabled"}))
            await ws.close(code=4404)
            return
        await ws.accept()
        session_id: Optional[str] = None
        meta: Dict[str, Any] = {}
        seen_audio = False
        try:
            while True:
                message = await ws.receive()
                if "bytes" in message and message["bytes"] is not None:
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    append_voice_chunk(session_id, message["bytes"])
                    seen_audio = True
                    await ws.send_text(json.dumps({"type": "ack", "event": "chunk"}))
                    continue
                if "text" not in message:
                    continue
                try:
                    payload = json.loads(message["text"])
                except Exception:
                    await ws.send_text(json.dumps({"type": "error", "error": "invalid_json"}))
                    continue
                event = payload.get("event")
                if event == "start":
                    session_id = payload.get("session_id")
                    meta = payload.get("meta", {}) if isinstance(payload.get("meta"), dict) else {}
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    await ws.send_text(json.dumps({"type": "ack", "event": "start"}))
                    continue
                if event == "chunk":
                    session_id = payload.get("session_id") or session_id
                    if not session_id:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_session_id"}))
                        continue
                    encoded = payload.get("data", "")
                    if not encoded:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_data"}))
                        continue
                    try:
                        chunk = base64.b64decode(encoded)
                    except Exception:
                        await ws.send_text(json.dumps({"type": "error", "error": "invalid_base64"}))
                        continue
                    append_voice_chunk(session_id, chunk, payload.get("format", "raw"))
                    seen_audio = True
                    await ws.send_text(json.dumps({"type": "ack", "event": "chunk"}))
                    continue
                if event == "stt":
                    session_id = payload.get("session_id") or session_id
                    transcript = payload.get("content", "")
                    if not session_id or not transcript:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    meta = payload.get("meta", meta) if isinstance(payload.get("meta"), dict) else meta
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "stt", "content": transcript, "meta": meta},
                    )
                    await ws.send_text(json.dumps({"type": "ack", "event": "stt"}))
                    continue
                if event == "tts":
                    session_id = payload.get("session_id") or session_id
                    text = payload.get("content", "")
                    if not session_id or not text:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "tts", "content": text, "meta": meta},
                    )
                    tts_cfg = config["gateway"].get("voice", {}).get("tts", {})
                    if tts_cfg.get("mode") == "webhook" and tts_cfg.get("webhook_url"):
                        await post_json(
                            tts_cfg["webhook_url"],
                            {"session_id": session_id, "text": text, "meta": meta},
                        )
                    await ws.send_text(json.dumps({"type": "ack", "event": "tts"}))
                    continue
                if event == "text":
                    session_id = payload.get("session_id") or session_id
                    content = payload.get("content", "")
                    if not session_id or not content:
                        await ws.send_text(json.dumps({"type": "error", "error": "missing_content"}))
                        continue
                    meta = payload.get("meta", meta) if isinstance(payload.get("meta"), dict) else meta
                    meta.setdefault("agent", "orchestrator")
                    meta.setdefault("channel_id", "voice")
                    meta.setdefault("scope", "dm")
                    meta.setdefault("approved", True)
                    persist_voice_text(
                        session_id,
                        {"ts": timestamp(), "event": "text", "content": content, "meta": meta},
                    )
                    asyncio.create_task(process_inbound(session_id, content, meta, config))
                    await ws.send_text(json.dumps({"type": "ack", "event": "text"}))
                    continue
                if event == "end":
                    if seen_audio:
                        stt_cfg = config["gateway"].get("voice", {}).get("stt", {})
                        if stt_cfg.get("mode") == "webhook" and stt_cfg.get("webhook_url"):
                            await post_json(
                                stt_cfg["webhook_url"],
                                {"session_id": session_id, "meta": meta},
                            )
                    await ws.send_text(json.dumps({"type": "ack", "event": "end"}))
                    continue
                await ws.send_text(json.dumps({"type": "error", "error": "unknown_event"}))
        except WebSocketDisconnect:
            return

    webchat_dir = Path(__file__).resolve().parent / "gateway_webchat"
    if webchat_dir.exists():
        app.mount("/webchat", StaticFiles(directory=str(webchat_dir), html=True), name="webchat")

    return app


def is_authorized(ws: WebSocket, config: Dict[str, Any]) -> Tuple[bool, Optional[str]]:
    mode = config["gateway"]["auth"].get("mode", "token")
    if mode == "token":
        token = config["gateway"]["auth"].get("token", {}).get("value", "")
        if not token:
            return True, None
        header = ws.headers.get("authorization") or ""
        if header.strip() == f"Bearer {token}":
            return True, None
        allow_query = config["gateway"]["auth"].get("token", {}).get("allow_query_param", False)
        if allow_query:
            query_token = ws.query_params.get("token", "")
            return (query_token == token), None
        return False, "invalid_token"
    if mode == "password":
        password = config["gateway"]["auth"].get("password", {}).get("value", "")
        if not password:
            return True, None
        header = ws.headers.get("authorization") or ""
        if not header.lower().startswith("basic "):
            return False, "missing_basic"
        try:
            import base64
            encoded = header.split(" ", 1)[1].strip()
            decoded = base64.b64decode(encoded).decode("utf-8")
        except Exception:
            return False, "invalid_basic"
        # Expect "username:password" format; only password is checked in v0.1
        if ":" not in decoded:
            return False, "invalid_basic"
        _user, pwd = decoded.split(":", 1)
        return (pwd == password), None
    return True, None


def ensure_session(session_id: str) -> None:
    if session_id not in STATE.sessions:
        STATE.sessions[session_id] = []
        STATE.sessions_active = len(STATE.sessions)
        STATE.session_version += 1
    if session_id not in STATE.session_locks:
        STATE.session_locks[session_id] = asyncio.Lock()


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
    ws: WebSocket,
    raw: str,
    seq_state: Dict[str, int],
    config: Dict[str, Any],
    conn_id: str,
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

    if method == "gateway.connect":
        protocol = params.get("protocol", "")
        if not protocol:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing protocol")
            return
        result = {
            "protocol": "v0.1",
            "server_time": timestamp(),
            "stateVersion": STATE.session_version,
            "sessions_active": STATE.sessions_active,
        }
        await send_result(ws, req_id, result)
        await send_presence(ws, seq_state)
        return

    if method == "chat.history":
        session_id = params.get("session_id", "")
        if not session_id:
            await send_error(ws, req_id, "INVALID_REQUEST", "Missing session_id")
            return
        ensure_session(session_id)
        STATE.conn_sessions[conn_id] = session_id
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
        STATE.conn_sessions[conn_id] = session_id
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
        if not is_message_allowed(meta, config):
            await send_error(ws, req_id, "POLICY_BLOCKED", "Message blocked by policy")
            audit_log({"ts": timestamp(), "event": "policy_block", "session_id": session_id})
            return
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
            persist_approval(run_id, pending)
            audit_log({"ts": timestamp(), "event": "approval_requested", "run_id": run_id})
            await send_error(ws, req_id, "APPROVAL_REQUIRED", "Execution approval required")
            return
        await send_result(ws, req_id, {"message_id": entry["message_id"], "session_id": session_id, "run_id": run_id})
        task = asyncio.create_task(
            dispatch_with_lock(ws, seq_state, session_id, run_id, meta, content, config)
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
        STATE.conn_sessions[conn_id] = session_id
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
    mode = meta.get("mode", "agent")
    if mode == "squad":
        squad = meta.get("squad", "build")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-squad"), "deploy", squad, content]
    elif mode == "combo":
        combo = meta.get("combo", "envision")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-combo"), "run", combo, content]
    elif mode == "playbook":
        playbook = meta.get("playbook", "zero-to-one")
        cmd = [str(Path.home() / ".euxis" / "bin" / "euxis-playbook"), "run", playbook, content]
    else:
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

    output_lines: List[str] = []

    async def stream_stdout() -> None:
        assert proc.stdout
        async for line in proc.stdout:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            output_lines.append(text)
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    async def stream_stderr() -> None:
        assert proc.stderr
        async for line in proc.stderr:
            text = line.decode("utf-8", errors="ignore").rstrip()
            if not text:
                continue
            output_lines.append(text)
            await send_agent_event(ws, seq_state, run_id, session_id, agent_id, "stream", text)

    await asyncio.gather(stream_stdout(), stream_stderr())
    await proc.wait()
    status = "final" if proc.returncode == 0 else "error"
    final_content = "\n".join(output_lines[-200:]) if output_lines else f"exit {proc.returncode}"
    await send_agent_event(ws, seq_state, run_id, session_id, agent_id, status, final_content)
    STATE.running.pop(run_id, None)


async def dispatch_with_lock(
    ws: WebSocket,
    seq_state: Dict[str, int],
    session_id: str,
    run_id: str,
    meta: Dict[str, Any],
    content: str,
    config: Dict[str, Any],
) -> None:
    lock = STATE.session_locks.setdefault(session_id, asyncio.Lock())
    async with lock:
        await dispatch_agent(ws, seq_state, session_id, run_id, meta, content, config)


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


async def cron_loop(config: Dict[str, Any]) -> None:
    try:
        while True:
            now = time.time()
            changed = False
            for job in STATE.cron_jobs:
                every = int(job.get("every_seconds", 0))
                if every <= 0:
                    continue
                last_run = float(job.get("last_run", 0))
                if now - last_run < every:
                    continue
                job["last_run"] = now
                changed = True
                meta = job.get("meta", {})
                session_id = job.get("session_id", "")
                content = job.get("content", "")
                if not session_id or not content:
                    continue
                asyncio.create_task(process_inbound(session_id, content, meta, config))
            if changed:
                persist_cron_jobs(STATE.cron_jobs)
            await asyncio.sleep(1)
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
    if status in {"final", "error"}:
        await notify_webhooks(payload)
        await deliver_to_channel(session_id, content)


async def notify_webhooks(payload: Dict[str, Any]) -> None:
    hooks = STATE.webhooks or STATE.config.get("gateway", {}).get("webhooks", [])
    if not hooks:
        return
    event_name = f"{payload.get('event')}.{payload.get('data', {}).get('status', '')}"
    body = json.dumps(payload).encode("utf-8")
    for hook in hooks:
        url = hook.get("url")
        events = hook.get("events", ["agent.final", "agent.error"])
        if not url or event_name not in events:
            continue
        for attempt in range(3):
            req = urlrequest.Request(
                url,
                data=body,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            try:
                await asyncio.to_thread(urlrequest.urlopen, req, 5)
                break
            except urlerror.URLError:
                await asyncio.sleep(0.5 * (attempt + 1))
                continue


async def post_json(url: str, payload: Dict[str, Any]) -> None:
    body = json.dumps(payload).encode("utf-8")
    req = urlrequest.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        await asyncio.to_thread(urlrequest.urlopen, req, 10)
    except urlerror.URLError:
        return


def split_message(content: str, max_len: int) -> List[str]:
    if max_len <= 0:
        return [content]
    if len(content) <= max_len:
        return [content]
    chunks: List[str] = []
    start = 0
    while start < len(content):
        end = min(start + max_len, len(content))
        chunks.append(content[start:end])
        start = end
    return chunks


async def deliver_to_channel(session_id: str, content: str) -> None:
    meta = load_session_meta(session_id)
    channel_id = meta.get("channel_id", "webchat")
    if channel_id == "webchat":
        return
    adapter = STATE.adapters.get(channel_id)
    if not adapter:
        return
    channel_cfg = STATE.config.get("gateway", {}).get("channels", {}).get(channel_id, {})
    max_len = int(channel_cfg.get("max_length", 3500))
    for chunk in split_message(content, max_len):
        try:
            adapter.send(chunk, session_id)
        except Exception:
            continue


def validate_canvas(state: Dict[str, Any]) -> List[str]:
    errors: List[str] = []
    if "widgets" in state and not isinstance(state.get("widgets"), list):
        errors.append("widgets must be a list")
    for idx, widget in enumerate(state.get("widgets", [])):
        if not isinstance(widget, dict):
            errors.append(f"widget[{idx}] must be an object")
            continue
        if "type" not in widget:
            errors.append(f"widget[{idx}] missing type")
    return errors


def is_exec_allowed(meta: Dict[str, Any], config: Dict[str, Any]) -> bool:
    exec_cfg = config["gateway"].get("exec", {})
    policy = exec_cfg.get("policy", "allowlist")
    ask = exec_cfg.get("ask", "on-miss")
    ask_fallback = exec_cfg.get("ask_fallback", "deny")
    elevated_mode = exec_cfg.get("elevated", "ask")
    allowlist = set(exec_cfg.get("allowlist", []))
    approved = bool(meta.get("approved"))
    elevated = meta.get("elevated") == "full"
    agent_id = meta.get("agent", "")

    if policy == "deny":
        return False
    if policy == "full":
        return True

    if elevated:
        if elevated_mode == "full":
            return True
        if elevated_mode == "off":
            return False

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


def is_message_allowed(meta: Dict[str, Any], config: Dict[str, Any]) -> bool:
    scope = meta.get("scope", "dm")
    channel_id = meta.get("channel_id", "")
    policy = config["gateway"].get("policy", {})
    mention_required = policy.get("mention_required", True)
    allowlist = set(policy.get("allowlist", []))
    mentions = meta.get("mentions_bot", False)
    sender = meta.get("sender", "")

    if scope == "dm":
        return True
    if allowlist and sender not in allowlist:
        return False
    if mention_required and not mentions:
        return False
    return True


async def process_inbound(session_id: str, content: str, meta: Dict[str, Any], config: Dict[str, Any]) -> None:
    ensure_session(session_id)
    entry = append_message(session_id, "user", content)
    session_meta = load_session_meta(session_id)
    session_meta.update(
        {
            "session_id": session_id,
            "channel_id": meta.get("channel_id", "unknown"),
            "scope": meta.get("scope", "dm"),
            "owner": meta.get("agent", "architect"),
            "updated_at": timestamp(),
        }
    )
    persist_session_meta(session_id, session_meta)
    if not is_message_allowed(meta, config):
        audit_log({"ts": timestamp(), "event": "policy_block", "session_id": session_id})
        return
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
        persist_approval(run_id, pending)
        audit_log({"ts": timestamp(), "event": "approval_requested", "run_id": run_id})
        return
    if not STATE.connections:
        return
    conn_id, ws = next(iter(STATE.connections.items()))
    seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
    task = asyncio.create_task(dispatch_with_lock(ws, seq_state, session_id, run_id, meta, content, config))
    STATE.running[run_id] = task
    STATE.conn_seq[conn_id] = seq_state["value"]


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
        status = {"health": resp.text, "channels": list(config["gateway"].get("channels", {}).keys())}
        print(json.dumps(status, indent=2))
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
