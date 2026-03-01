"""Multi-stage admission pipeline for skill verification."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from euxis_bridge.core.provenance import ProvenanceChain, ProvenanceRecord
from euxis_bridge.core.reputation import AuthorReputation, ReputationStore
from euxis_bridge.core.static_analysis import AnalysisReport, analyze_skill_directory
from euxis_bridge.core.verification import verify_skill_signature


@dataclass(frozen=True, slots=True)
class AdmissionResult:
    admitted: bool
    stages_passed: tuple[str, ...]
    stages_failed: tuple[str, ...]
    findings: tuple[Any, ...] = ()
    reputation: AuthorReputation | None = None
    provenance: ProvenanceRecord | None = None


class AdmissionPipeline:
    """Orchestrates multi-stage skill admission."""

    def __init__(
        self,
        public_key_path: str | None = None,
        reputation_store: ReputationStore | None = None,
        reputation_threshold: float = 0.3,
        provenance_builder_id: str = "euxis-bridge",
        require_signature: bool = False,
        require_reputation: bool = False,
    ) -> None:
        self.public_key_path = public_key_path
        self.reputation_store = reputation_store
        self.reputation_threshold = reputation_threshold
        self.provenance_chain = ProvenanceChain(provenance_builder_id)
        self.require_signature = require_signature
        self.require_reputation = require_reputation

    def evaluate(
        self,
        skill_dir: Path,
        entrypoint: Path | None = None,
        author_id: str = "unknown",
        source_uri: str = "",
    ) -> AdmissionResult:
        passed: list[str] = []
        failed: list[str] = []
        all_findings: list[Any] = []
        reputation: AuthorReputation | None = None
        provenance: ProvenanceRecord | None = None

        # Stage 1: Signature verification (optional)
        if self.require_signature and entrypoint is not None and self.public_key_path:
            if verify_skill_signature(entrypoint, self.public_key_path):
                passed.append("signature")
            else:
                failed.append("signature")
                return AdmissionResult(
                    admitted=False, stages_passed=tuple(passed), stages_failed=tuple(failed),
                )

        # Stage 2: Static analysis
        report: AnalysisReport = analyze_skill_directory(skill_dir)
        all_findings.extend(report.findings)
        if report.passed:
            passed.append("static_analysis")
        else:
            failed.append("static_analysis")
            return AdmissionResult(
                admitted=False, stages_passed=tuple(passed), stages_failed=tuple(failed),
                findings=tuple(all_findings),
            )

        # Stage 3: Provenance
        provenance = self.provenance_chain.generate(skill_dir.name, skill_dir, source_uri)
        passed.append("provenance")

        # Stage 4: Reputation check (optional)
        if self.require_reputation and self.reputation_store is not None:
            reputation = self.reputation_store.get_reputation(author_id)
            if self.reputation_store.check_threshold(author_id, self.reputation_threshold):
                passed.append("reputation")
            else:
                failed.append("reputation")
                return AdmissionResult(
                    admitted=False, stages_passed=tuple(passed), stages_failed=tuple(failed),
                    findings=tuple(all_findings), reputation=reputation, provenance=provenance,
                )

        return AdmissionResult(
            admitted=True, stages_passed=tuple(passed), stages_failed=tuple(failed),
            findings=tuple(all_findings), reputation=reputation, provenance=provenance,
        )
