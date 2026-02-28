import json
from pathlib import Path
from gateway.identity import AgentIdentity, IdentityManager

def test_agent_identity_to_dict():
    identity = AgentIdentity("test", "Test Agent", "elevated", {"a"}, {"b"}, {"c": 1})
    data = identity.to_dict()
    assert data["agent_id"] == "test"
    assert "a" in data["permissions"]

def test_identity_manager(tmp_path):
    manager = IdentityManager(tmp_path)
    # Should create defaults
    assert manager.get_identity("architect").trust_level == "trusted"
    # Should handle unknown
    assert manager.get_identity("unknown").trust_level == "restricted"
    
    # Save and reload
    manager.identities["new"] = AgentIdentity("new", "New")
    manager.save()
    
    manager2 = IdentityManager(tmp_path)
    assert manager2.get_identity("new").trust_level == "restricted"

def test_check_permission(tmp_path):
    manager = IdentityManager(tmp_path)
    assert manager.check_permission("architect", "any_perm") is True # * permission
    assert manager.check_permission("researcher", "web_search") is True
    assert manager.check_permission("researcher", "admin_perm") is False

def test_load_failure(tmp_path, monkeypatch):
    manager = IdentityManager(tmp_path)
    # write bad json
    (tmp_path / "identities.json").write_text("not json")
    
    # Reload should fail gracefully
    manager2 = IdentityManager(tmp_path)
    # Defaults should still be present
    assert manager2.get_identity("architect").trust_level == "trusted"
