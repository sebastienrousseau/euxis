"""Core bridge logic, independent of platform adapters."""

from euxis_bridge.core.admission import AdmissionPipeline, AdmissionResult
from euxis_bridge.core.audit import AuditEntry, AuditLogger
from euxis_bridge.core.decorators import openclaw_skill
from euxis_bridge.core.importer import ClawHubImporter, ImportReport
from euxis_bridge.core.models import BridgedSkill
from euxis_bridge.core.node_bridge import NodeSkillBridge, NodeSkillError
from euxis_bridge.core.policy import (
    FilesystemPolicy,
    NetworkPolicy,
    ResourceLimits,
    SkillExecutionPolicy,
)
from euxis_bridge.core.provenance import ProvenanceChain, ProvenanceRecord
from euxis_bridge.core.reputation import AuthorReputation, ReputationStore
from euxis_bridge.core.static_analysis import (
    AnalysisFinding,
    AnalysisReport,
    analyze_file,
    analyze_skill_directory,
)
from euxis_bridge.core.verification import (
    SkillVerificationError,
    require_verified,
    verify_skill_signature,
)

__all__ = [
    "AdmissionPipeline",
    "AdmissionResult",
    "AnalysisFinding",
    "AnalysisReport",
    "AuditEntry",
    "AuditLogger",
    "AuthorReputation",
    "BridgedSkill",
    "ClawHubImporter",
    "FilesystemPolicy",
    "ImportReport",
    "NetworkPolicy",
    "NodeSkillBridge",
    "NodeSkillError",
    "ProvenanceChain",
    "ProvenanceRecord",
    "ReputationStore",
    "ResourceLimits",
    "SkillExecutionPolicy",
    "SkillVerificationError",
    "analyze_file",
    "analyze_skill_directory",
    "openclaw_skill",
    "require_verified",
    "verify_skill_signature",
]
