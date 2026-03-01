"""Public package API for euxis-identity."""

from .core.attestation import Attestation, create_attestation, verify_attestation
from .core.credentials import Claim, Proof, VerifiableCredential, issue_credential, verify_credential
from .core.did import DIDDocument, ServiceEndpoint, VerificationMethod, create_did, create_did_document, resolve_did
from .core.erc8004 import ERC8004AgentCard, generate_agent_card, serialize_agent_card
from .core.registry import AgentIdentity, IdentityRegistry, InMemoryIdentityRegistry
from .core.service import CoreService
from .platform.factory import resolve_platform_ops

__all__ = [
    "AgentIdentity", "Attestation", "Claim", "CoreService",
    "DIDDocument", "ERC8004AgentCard", "IdentityRegistry",
    "InMemoryIdentityRegistry", "Proof", "ServiceEndpoint",
    "VerifiableCredential", "VerificationMethod",
    "create_attestation", "create_did", "create_did_document",
    "issue_credential", "resolve_platform_ops", "verify_attestation",
    "verify_credential", "generate_agent_card", "serialize_agent_card",
]
