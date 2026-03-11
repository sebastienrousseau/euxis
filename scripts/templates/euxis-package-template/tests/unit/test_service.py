from euxis_template.core.service import CoreService
from euxis_template.platform.contracts import PlatformInfo


class StubOps:
    def platform_info(self) -> PlatformInfo:
        return PlatformInfo(platform="linux", runtime="native")

    def path_separator(self) -> str:
        return "/"

    def join_workspace(self, relative_path: str) -> str:
        return f"~/.euxis/{relative_path.strip('/')}"


def test_health_snapshot() -> None:
    snapshot = CoreService(ops=StubOps()).health_snapshot()
    assert snapshot == {
        "platform": "linux",
        "runtime": "native",
        "path_separator": "/",
    }


def test_normalize_workspace_path() -> None:
    result = CoreService(ops=StubOps()).normalize_workspace_path("projects/demo")
    assert result == "~/.euxis/projects/demo"
