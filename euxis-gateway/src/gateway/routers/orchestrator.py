
from fastapi import APIRouter

def setup_orchestrator(router: APIRouter, deps: dict):
    globals().update(deps)

    @router.get("/approvals")
    async def approvals(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        return JSONResponse({"pending": list(STATE.pending_approvals.values())})

    @router.get("/automation/cron")
    async def cron_list(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        return JSONResponse({"jobs": STATE.cron_jobs})

    @router.post("/automation/cron")
    async def cron_create(payload: Dict[str, Any], request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

    @router.delete("/automation/cron/{job_id}")
    async def cron_delete(job_id: str, request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        before = len(STATE.cron_jobs)
        STATE.cron_jobs = [job for job in STATE.cron_jobs if job.get("job_id") != job_id]
        persist_cron_jobs(STATE.cron_jobs)
        if len(STATE.cron_jobs) == before:
            return JSONResponse({"status": "not_found"}, status_code=404)
        return JSONResponse({"status": "deleted"})


    @router.get("/runs")
    async def runs(request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        runs = [p.stem for p in runs_dir().glob("*.jsonl")]
        return JSONResponse({"runs": runs})

    @router.get("/runs/{run_id}")
    async def run_detail(run_id: str, request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
        return JSONResponse({"run_id": run_id, "events": load_run_events(run_id)})

    @router.post("/approvals/{run_id}/approve")
    async def approve(run_id: str, request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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
        # Dispatch concurrently to all active connections spanning this session
        async with asyncio.TaskGroup() as tg:
            for conn_id, ws in list(STATE.connections.items()):
                if STATE.conn_sessions.get(conn_id) == pending["session_id"] or not STATE.conn_sessions.get(conn_id):
                    seq_state = {"value": STATE.conn_seq.get(conn_id, 0)}
                    task = tg.create_task(
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
        return JSONResponse({"status": "approved"})

    @router.post("/approvals/{run_id}/reject")
    async def reject(run_id: str, request: Request) -> JSONResponse:
        allowed, reason = is_http_authorized(request, config)
        if not allowed:
            return JSONResponse({"status": "unauthorized", "reason": reason}, status_code=401)
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

