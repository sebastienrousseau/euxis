from euxis_bridge.core.decorators import openclaw_skill


def test_openclaw_skill_decorator_sets_metadata() -> None:
    @openclaw_skill(name="Browser Use", slug="browser-use")
    def wrapped() -> str:
        return "ok"

    metadata = getattr(wrapped, "_openclaw_skill")
    assert metadata == {"name": "Browser Use", "slug": "browser-use"}
    assert wrapped() == "ok"
