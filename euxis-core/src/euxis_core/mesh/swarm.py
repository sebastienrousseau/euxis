# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Swarm Orchestrator for Multi-Agent Playbook execution."""

from __future__ import annotations

import json
import logging
import uuid
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

LOGGER = logging.getLogger("euxis.core.swarm")

from .router import FinOpsRouter
from euxis_core.resilience import CircuitBreaker, RetryPolicy
from euxis_core.runtime import (
    GatewayWebSocketClient,
    gather_awaitables,
    run_async_with_resilience,
)

@dataclass
class SwarmTask:
    """A single task within a swarm phase."""
    agent_id: str
    template: str
    status: str = "pending"
    result: Optional[str] = None
    run_id: str = field(default_factory=lambda: f"run_{uuid.uuid4().hex[:8]}")
    complexity: str = "medium"

class SwarmOrchestrator:
    """Orchestrates complex multi-agent workflows defined in playbooks."""

    def __init__(self, gateway_url: str, token: Optional[str] = None):
        # Convert http(s) to ws(s) for gateway communication
        self.gateway_url = gateway_url.replace("http://", "ws://").replace("https://", "wss://")
        self.token = token
        self.history: List[Dict[str, Any]] = []
        self.router = FinOpsRouter()
        self.ws_client = GatewayWebSocketClient(self.gateway_url, token=self.token)
        self.connection_retry_policy = RetryPolicy(
            max_attempts=3, base_delay_seconds=0.1, max_delay_seconds=1.0, jitter_ratio=0.0
        )
        self.connection_breaker = CircuitBreaker(failure_threshold=3, recovery_timeout_seconds=5.0)
        self.simulation_mode = "localhost" in self.gateway_url

    async def execute_playbook(self, playbook: Dict[str, Any], goal: str):
        """Execute all phases of a playbook."""
        LOGGER.info(f"Starting swarm playbook: {playbook.get('name')}")
        
        try:
            for phase in playbook.get("phases", []):
                await self._execute_phase(phase, goal)
        finally:
            await self.ws_client.close()
            
        LOGGER.info("Swarm playbook execution complete.")
        return self.history

    async def _execute_phase(self, phase: Dict[str, Any], goal: str):
        """Execute a single phase (parallel or sequential)."""
        name = phase.get("name", "Unknown Phase")
        mode = phase.get("mode", "sequential")
        delegates = phase.get("delegates", [])
        
        LOGGER.info(f"Executing phase: {name} (mode: {mode})")
        
        tasks = []
        for d in delegates:
            content = d.get("task_template", "").replace("${goal}", goal)
            # Replace history if present
            if "${history}" in content:  # pragma: no cover
                content = content.replace("${history}", json.dumps(self.history))  # pragma: no cover
            
            tasks.append(SwarmTask(d.get("agent"), content))

        if mode == "parallel":
            await gather_awaitables(self._run_task(t) for t in tasks)
        else:
            for t in tasks:
                await self._run_task(t)

    async def _ensure_connection(self):
        """Lazily establish a WebSocket connection to the gateway."""
        if self.ws_client.closed:
            try:
                await run_async_with_resilience(
                    self.ws_client.connect,
                    retry_policy=self.connection_retry_policy,
                    circuit_breaker=self.connection_breaker,
                )
            except Exception as e:
                status_code = self.ws_client.invalid_status_code(e)
                if status_code == 401:
                    LOGGER.error(f"Gateway authentication failed (401). Check your token.")
                else:
                    LOGGER.error(f"Failed to connect to gateway at {self.gateway_url}: {e}")
                raise

    async def _run_task(self, task: SwarmTask):
        """Dispatch a task to the Euxis Gateway via WebSocket and wait for result."""
        provider = self.router.select_provider(task.complexity)
        LOGGER.info(f"Dispatching task to agent {task.agent_id} via {provider}...")
        task.status = "running"

        if self.simulation_mode:
            task.status = "completed"
            task.result = f"Simulated result from {task.agent_id}"
            self.router.track_usage(provider, 500)
            self.history.append(
                {
                    "agent": task.agent_id,
                    "provider": provider,
                    "task": task.template,
                    "result": task.result,
                }
            )
            return
        
        await self._ensure_connection()
        
        req_id = task.run_id
        payload = {
            "type": "request",
            "id": req_id,
            "method": "chat.send",
            "params": {
                "session_id": f"swarm_{req_id}",
                "role": "user",
                "content": task.template,
                "meta": {
                    "agent": task.agent_id, 
                    "provider": provider,
                    "approved": True
                }
            }
        }
        
        try:
            # Send the request
            await self.ws_client.send(json.dumps(payload))
            
            # Read messages until we get a final event for this run_id
            async for message in self.ws_client.iter_messages():
                data = json.loads(message)
                
                # Check for error result
                if data.get("type") == "result" and data.get("id") == req_id:
                    if "error" in data:
                        raise Exception(f"Gateway error: {data['error']}")
                
                # Check for agent events
                if data.get("type") == "event" and data.get("event") == "agent":
                    evt_data = data.get("data", {})
                    # Only care about our run_id
                    if evt_data.get("run_id") == req_id:
                        status = evt_data.get("status")
                        if status == "final":
                            task.status = "completed"
                            task.result = evt_data.get("content")
                            # Extract token usage if available from metadata
                            tokens = evt_data.get("meta", {}).get("usage", {}).get("total_tokens", 500)
                            self.router.track_usage(provider, tokens)
                            break
                        elif status == "error":
                            raise Exception(f"Agent error: {evt_data.get('content')}")
            
            if task.status != "completed":
                raise Exception("WebSocket closed before task completion")

            self.history.append({
                "agent": task.agent_id,
                "provider": provider,
                "task": task.template,
                "result": task.result
            })
            
        except Exception as e:
            LOGGER.error(f"Task {task.run_id} failed: {e}")
            task.status = "failed"
            task.result = f"Error: {e}"
            self.history.append({
                "agent": task.agent_id,
                "provider": provider,
                "task": task.template,
                "result": task.result
            })
