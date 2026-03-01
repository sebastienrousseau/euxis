"""Public package API for euxis-a2a."""

from .core.agent_card import AgentCard, AuthSchema, Capability, generate_agent_card, serialize_card, validate_card
from .core.message import A2AMessage, Artifact, ContentPart
from .core.server import A2AServerHandler
from .core.service import CoreService
from .core.task import A2ATask, TaskStatus, create_task, transition_task
from .core.transport import A2ADiscovery, A2ATransport
from .platform.factory import resolve_platform_ops
from .runtime.http_transport import HttpA2ADiscovery, HttpA2ATransport

__all__ = [
    "A2AMessage", "A2AServerHandler", "A2ATask", "A2ATransport",
    "AgentCard", "Artifact", "AuthSchema", "Capability", "ContentPart",
    "CoreService", "HttpA2ADiscovery", "HttpA2ATransport", "TaskStatus",
    "A2ADiscovery",
    "create_task", "generate_agent_card", "resolve_platform_ops",
    "serialize_card", "transition_task", "validate_card",
]
