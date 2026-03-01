"""Tests for per-agent encrypted memory store."""

from pathlib import Path

import pytest

from crypto_lib.key_management import generate_key
from euxis_core.encrypted_memory import (
    EncryptedMemoryEntry,
    EncryptedMemoryStore,
    MemoryTier,
)


@pytest.fixture()
def master_key() -> bytes:
    return generate_key(32)


@pytest.fixture()
def store(tmp_path: Path, master_key: bytes) -> EncryptedMemoryStore:
    return EncryptedMemoryStore(tmp_path / "mem", master_key, "did:euxis:agent-1")


def test_store_and_retrieve_roundtrip(store: EncryptedMemoryStore) -> None:
    entry = store.store("hello world", tier=MemoryTier.HOT)
    assert isinstance(entry, EncryptedMemoryEntry)
    assert entry.tier == "hot"
    assert entry.agent_did == "did:euxis:agent-1"
    result = store.retrieve(entry.entry_id)
    assert result == "hello world"


def test_retrieve_missing_raises(store: EncryptedMemoryStore) -> None:
    with pytest.raises(KeyError, match="memory entry not found"):
        store.retrieve("nonexistent-id")


def test_tier_bound_aad_prevents_cross_tier(
    tmp_path: Path, master_key: bytes,
) -> None:
    store = EncryptedMemoryStore(tmp_path / "mem", master_key, "did:euxis:agent-1")
    entry = store.store("secret data", tier=MemoryTier.HOT)

    # Retrieve from the correct tier works
    results = store.retrieve_tier(MemoryTier.HOT)
    assert len(results) == 1
    assert results[0] == "secret data"

    # Different tier returns empty
    results = store.retrieve_tier(MemoryTier.RELEVANT)
    assert len(results) == 0


def test_multiple_agents_isolated(tmp_path: Path, master_key: bytes) -> None:
    store1 = EncryptedMemoryStore(tmp_path / "mem", master_key, "did:euxis:agent-1")
    store2 = EncryptedMemoryStore(tmp_path / "mem", master_key, "did:euxis:agent-2")

    e1 = store1.store("agent 1 data")
    e2 = store2.store("agent 2 data")

    assert store1.retrieve(e1.entry_id) == "agent 1 data"
    assert store2.retrieve(e2.entry_id) == "agent 2 data"

    # Agent 2 cannot find agent 1's entries
    with pytest.raises(KeyError):
        store2.retrieve(e1.entry_id)


def test_destroy_agent_keys_prevents_decryption(store: EncryptedMemoryStore) -> None:
    entry = store.store("before destroy")
    store.destroy_agent_keys()
    with pytest.raises(Exception):
        store.retrieve(entry.entry_id)


def test_export_returns_ciphertext(store: EncryptedMemoryStore) -> None:
    store.store("data1", tier=MemoryTier.HOT)
    store.store("data2", tier=MemoryTier.HOT)
    store.store("data3", tier=MemoryTier.RELEVANT)

    hot_entries = store.export_tier_encrypted(MemoryTier.HOT)
    assert len(hot_entries) == 2
    assert all(isinstance(e, EncryptedMemoryEntry) for e in hot_entries)
    assert all(e.ciphertext != "data1" for e in hot_entries)  # Not plaintext

    relevant_entries = store.export_tier_encrypted(MemoryTier.RELEVANT)
    assert len(relevant_entries) == 1


def test_retrieve_tier_with_limit(store: EncryptedMemoryStore) -> None:
    for i in range(5):
        store.store(f"item-{i}", tier=MemoryTier.HOT)
    results = store.retrieve_tier(MemoryTier.HOT, limit=3)
    assert len(results) == 3


def test_memory_tier_constants() -> None:
    assert MemoryTier.HOT == "hot"
    assert MemoryTier.RELEVANT == "relevant"
    assert MemoryTier.CROSS_AGENT == "cross_agent"


def test_unicode_content(store: EncryptedMemoryStore) -> None:
    entry = store.store("日本語テスト 🧠")
    result = store.retrieve(entry.entry_id)
    assert result == "日本語テスト 🧠"
