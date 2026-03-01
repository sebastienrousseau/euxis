from pathlib import Path

from euxis_bridge.core.models import BridgedSkill


def test_bridged_skill_to_dict_and_source_path() -> None:
    skill = BridgedSkill(
        name="Browser Use",
        slug="browser-use",
        source_dir="/tmp/skills/browser-use",
        description="desc",
        runtime="node",
        entrypoint="/tmp/skills/browser-use/index.js",
        command=["node", "index.js"],
        tags=("web",),
        metadata={"a": 1},
    )

    payload = skill.to_dict()
    assert payload["tags"] == ["web"]
    assert isinstance(skill.source_path, Path)
