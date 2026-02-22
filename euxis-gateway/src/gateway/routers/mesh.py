
from fastapi import APIRouter

def setup_mesh(router: APIRouter, deps: dict):
    globals().update(deps)

    @router.get("/sessions")
    async def sessions(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

    @router.get("/sessions/export")
    async def sessions_export(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        data: Dict[str, Any] = {}
        for session_id in STATE.sessions.keys():
            if not STATE.sessions[session_id]:
                STATE.sessions[session_id] = load_session_from_disk(session_id)
            data[session_id] = STATE.sessions[session_id]
        return JSONResponse({"sessions": data})

    @router.post("/sessions/import")
    async def sessions_import(payload: Dict[str, Any], request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

    @router.get("/sessions/{session_id}")
    async def session_detail(session_id: str, request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        ensure_session(session_id)
        if not STATE.sessions[session_id]:
            STATE.sessions[session_id] = load_session_from_disk(session_id)
        meta = load_session_meta(session_id)
        return JSONResponse({"session_id": session_id, "meta": meta, "messages": STATE.sessions[session_id]})

    @router.get("/canvas/{session_id}")
    async def canvas_get(session_id: str, request: Request) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        return JSONResponse({"session_id": session_id, "state": load_canvas_state(session_id)})

    @router.post("/canvas/{session_id}")
    async def canvas_set(session_id: str, payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        state = payload.get("state", {})
        if not isinstance(state, dict):
            return JSONResponse({"status": "invalid"}, status_code=400)
        persist_canvas_state(session_id, state)
        return JSONResponse({"status": "ok"})

    @router.post("/canvas/{session_id}/validate")
    async def canvas_validate(session_id: str, payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("canvas", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        state = payload.get("state", {})
        if not isinstance(state, dict):
            return JSONResponse({"status": "invalid"}, status_code=400)
        errors = validate_canvas(state)
        return JSONResponse({"status": "ok" if not errors else "invalid", "errors": errors})

    @router.post("/voice/wake")
    async def voice_wake(payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

    @router.post("/voice/talk")
    async def voice_talk(payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

    # SECURITY: Allowed audio file extensions for voice upload
    VOICE_ALLOWED_EXTENSIONS = {"wav", "mp3", "ogg", "webm", "m4a", "flac", "aac"}
    # SECURITY: Maximum file size for voice uploads (10MB)
    VOICE_MAX_FILE_SIZE = 10 * 1024 * 1024

    @router.post("/voice/upload")
    async def voice_upload(session_id: str, file: UploadFile = File(...), request: Request = None) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        # SECURITY: Authentication is ALWAYS required (removed optional check)
        if request is None:
            return JSONResponse({"status": "unauthorized", "reason": "missing_request"}, status_code=401)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        if not session_id:
            return JSONResponse({"status": "invalid"}, status_code=400)
        # SECURITY: Validate file extension against allowlist
        filename = file.filename or "audio.wav"
        suffix = filename.rsplit(".", 1)[-1].lower() if "." in filename else ""
        if suffix not in VOICE_ALLOWED_EXTENSIONS:
            return JSONResponse(
                {"status": "invalid", "reason": f"file_type_not_allowed: {suffix}"},
                status_code=400,
            )
        # SECURITY: Read file with size limit to prevent DoS
        max_size = config["gateway"].get("voice", {}).get("max_upload_size", VOICE_MAX_FILE_SIZE)
        chunks = []
        total_size = 0
        async for chunk in file.stream():
            total_size += len(chunk)
            if total_size > max_size:
                return JSONResponse(
                    {"status": "invalid", "reason": f"file_too_large: max {max_size} bytes"},
                    status_code=413,
                )
            chunks.append(chunk)
        blob = b"".join(chunks)
        # SECURITY: Validate content magic bytes for audio files
        if not _validate_audio_magic(blob, suffix):
            return JSONResponse(
                {"status": "invalid", "reason": "content_type_mismatch"},
                status_code=400,
            )
        path = persist_voice_blob(session_id, blob, suffix)
        return JSONResponse({"status": "ok", "path": str(path)})

    @router.post("/voice/tts")
    async def voice_tts(payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        session_id = payload.get("session_id", "")
        text = payload.get("content", "")
        if not session_id or not text:
            return JSONResponse({"status": "invalid"}, status_code=400)
        await push_voice_tts(session_id, text)
        tts_cfg = config["gateway"].get("voice", {}).get("tts", {})
        if tts_cfg.get("mode") == "webhook" and tts_cfg.get("webhook_url"):
            await post_json(tts_cfg["webhook_url"], {"session_id": session_id, "text": text})
        return JSONResponse({"status": "ok"})

    @router.post("/voice/stt")
    async def voice_stt(payload: Dict[str, Any], request: Request) -> JSONResponse:
        if not config["gateway"].get("voice", {}).get("enabled", True):
            return JSONResponse({"status": "disabled"}, status_code=404)
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        session_id = payload.get("session_id", "")
        audio_path = payload.get("audio_path", "")
        if not session_id:
            return JSONResponse({"status": "invalid"}, status_code=400)
        stt_cfg = config["gateway"].get("voice", {}).get("stt", {})
        if stt_cfg.get("mode") != "command" or not stt_cfg.get("command"):
            return JSONResponse({"status": "disabled"}, status_code=404)
        if not audio_path:
            blob = resolve_voice_blob(session_id, payload.get("format", "raw"))
            audio_path = str(blob) if blob else ""
        if not audio_path:
            return JSONResponse({"status": "invalid"}, status_code=400)
        transcript = await run_voice_command(
            stt_cfg.get("command", ""),
            {"audio_path": audio_path},
        )
        if transcript:
            persist_voice_text(
                session_id,
                {"ts": timestamp(), "event": "stt", "content": transcript, "meta": payload.get("meta", {})},
            )
        return JSONResponse({"status": "ok", "content": transcript})

    @router.post("/sessions/{session_id}/broadcast")
    async def session_broadcast(session_id: str, payload: Dict[str, Any], request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        message = payload.get("message", "")
        if not message:
            return JSONResponse({"status": "invalid"}, status_code=400)
        async with asyncio.TaskGroup() as tg:
            for conn_id, ws in list(STATE.connections.items()):
                if STATE.conn_sessions.get(conn_id) != session_id:
                    continue
                seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
                tg.create_task(
                    send_agent_event(ws, seq_state, f"run_broadcast_{int(time.time() * 1000)}", session_id, "gateway", "final", message)
                )
                STATE.conn_seq[conn_id] = seq_state["value"]
        return JSONResponse({"status": "ok"})



