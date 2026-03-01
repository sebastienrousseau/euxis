"""Tests for SkillExecutionPolicy and sub-policies."""

from euxis_bridge.core.policy import (
    FilesystemPolicy,
    NetworkPolicy,
    ResourceLimits,
    SkillExecutionPolicy,
)


def test_default_filesystem_policy() -> None:
    p = FilesystemPolicy()
    assert p.read_only is True
    assert p.write_paths == ()


def test_default_network_policy() -> None:
    p = NetworkPolicy()
    assert p.deny_all is True
    assert p.allowed_hosts == ()


def test_default_resource_limits() -> None:
    r = ResourceLimits()
    assert r.memory_mb == 256
    assert r.timeout_seconds == 20


def test_default_skill_execution_policy() -> None:
    p = SkillExecutionPolicy()
    assert p.require_signature is False
    assert p.public_key_path is None
    assert p.filesystem.read_only is True
    assert p.network.deny_all is True
    assert p.resources.memory_mb == 256
    assert p.output_schema is None
    assert p.audit_log_path is None


def test_permissive_factory() -> None:
    p = SkillExecutionPolicy.permissive()
    assert p.require_signature is False
    assert p.filesystem.read_only is False
    assert p.network.deny_all is False
    assert p.resources.memory_mb == 512
    assert p.resources.timeout_seconds == 60


def test_frozen_immutability() -> None:
    p = SkillExecutionPolicy()
    try:
        p.require_signature = True  # type: ignore[misc]
        raise AssertionError("Expected FrozenInstanceError")
    except AttributeError:
        pass


def test_custom_policy() -> None:
    p = SkillExecutionPolicy(
        require_signature=True,
        public_key_path="/keys/pub.pem",
        filesystem=FilesystemPolicy(read_only=False, write_paths=("/tmp",)),
        network=NetworkPolicy(deny_all=False, allowed_hosts=("api.example.com",)),
        resources=ResourceLimits(memory_mb=128, timeout_seconds=5),
        output_schema={"required": ["result"]},
        audit_log_path="/var/log/audit.jsonl",
    )
    assert p.require_signature is True
    assert p.public_key_path == "/keys/pub.pem"
    assert p.filesystem.write_paths == ("/tmp",)
    assert p.network.allowed_hosts == ("api.example.com",)
    assert p.resources.memory_mb == 128
    assert p.output_schema == {"required": ["result"]}
    assert p.audit_log_path == "/var/log/audit.jsonl"
