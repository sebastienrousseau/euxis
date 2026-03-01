"""Per-agent encrypted memory with tier-bound AAD using euxis-crypto."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import uuid
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import ClassVar


class MemoryTier:
    """Standard memory tier labels used as AAD for tier-bound encryption."""

    HOT: ClassVar[str] = "hot"
    RELEVANT: ClassVar[str] = "relevant"
    CROSS_AGENT: ClassVar[str] = "cross_agent"


@dataclass(frozen=True, slots=True)
class EncryptedMemoryEntry:
    """Immutable encrypted memory entry."""

    entry_id: str
    tier: str
    ciphertext: str  # Base64-encoded
    created_at: str
    agent_did: str


class EncryptedMemoryStore:
    """Per-agent encrypted memory store with tier-bound AAD.

    Each agent gets a derived key from the master key + agent DID.
    Memory entries are encrypted with AAD set to the tier name,
    preventing cross-tier decryption.
    """

    def __init__(self, store_dir: Path, master_key: bytes, agent_did: str) -> None:
        self._store_dir = store_dir
        self._master_key = master_key
        self._agent_did = agent_did
        self._agent_key = self._derive_agent_key(master_key, agent_did)
        self._agent_hash = hashlib.sha256(agent_did.encode("utf-8")).hexdigest()[:16]
        self._agent_dir = self._store_dir / self._agent_hash
        self._agent_dir.mkdir(parents=True, exist_ok=True)
        self._memory_file = self._agent_dir / "encrypted_memory.jsonl"

    def _derive_agent_key(self, master_key: bytes, agent_did: str) -> bytes:
        """Derive per-agent key using PBKDF2(master_key, salt=sha256(agent_did))."""
        try:
            from crypto_lib.core import encrypt_aad as _  # noqa: F401
            from crypto_lib.key_management import derive_key
        except ImportError:  # pragma: no cover
            raise RuntimeError("euxis-crypto is required for encrypted memory")

        salt = hashlib.sha256(agent_did.encode("utf-8")).digest()
        key, _ = derive_key(
            password=base64.b64encode(master_key).decode("ascii"),
            salt=salt,
            key_size=32,
            iterations=100_000,
        )
        return key

    def store(self, content: str, tier: str = MemoryTier.HOT) -> EncryptedMemoryEntry:
        """Encrypt and store a memory entry with tier-bound AAD."""
        from crypto_lib.core import encrypt_aad

        result = encrypt_aad(content, self._agent_key, aad=tier.encode("utf-8"))
        ciphertext_b64 = base64.b64encode(
            result.iv + result.ciphertext
        ).decode("ascii")

        entry = EncryptedMemoryEntry(
            entry_id=uuid.uuid4().hex,
            tier=tier,
            ciphertext=ciphertext_b64,
            created_at=datetime.now(tz=timezone.utc).isoformat(),
            agent_did=self._agent_did,
        )

        with self._memory_file.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(asdict(entry), separators=(",", ":")) + "\n")

        return entry

    def _decrypt_entry(self, entry: EncryptedMemoryEntry) -> str:
        """Decrypt a single memory entry using its tier as AAD."""
        from crypto_lib.core import EncryptionResult, decrypt_aad

        raw = base64.b64decode(entry.ciphertext)
        iv = raw[:12]
        ciphertext = raw[12:]

        enc_result = EncryptionResult(ciphertext=ciphertext, iv=iv)
        dec_result = decrypt_aad(enc_result, self._agent_key, aad=entry.tier.encode("utf-8"))
        return dec_result.to_string()

    def _load_entries(self) -> list[EncryptedMemoryEntry]:
        """Load all entries from the JSONL file."""
        if not self._memory_file.exists():
            return []
        entries: list[EncryptedMemoryEntry] = []
        for line in self._memory_file.read_text(encoding="utf-8").strip().splitlines():
            data = json.loads(line)
            entries.append(EncryptedMemoryEntry(**data))
        return entries

    def retrieve(self, entry_id: str) -> str:
        """Retrieve and decrypt a specific memory entry."""
        for entry in self._load_entries():
            if entry.entry_id == entry_id:
                return self._decrypt_entry(entry)
        raise KeyError(f"memory entry not found: {entry_id}")

    def retrieve_tier(self, tier: str, limit: int = 20) -> list[str]:
        """Retrieve and decrypt all entries for a given tier."""
        entries = [e for e in self._load_entries() if e.tier == tier]
        results: list[str] = []
        for entry in entries[:limit]:
            results.append(self._decrypt_entry(entry))
        return results

    def destroy_agent_keys(self) -> None:
        """Destroy the derived agent key, preventing further decryption."""
        self._agent_key = b"\x00" * 32

    def export_tier_encrypted(self, tier: str) -> list[EncryptedMemoryEntry]:
        """Export encrypted entries for a tier without decrypting."""
        return [e for e in self._load_entries() if e.tier == tier]
