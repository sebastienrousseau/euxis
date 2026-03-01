from euxis_bridge.platform.contracts import PlatformInfo
from euxis_bridge.platform.factory import resolve_platform_ops


def test_platform_contract_and_factory() -> None:
    info = PlatformInfo(platform="linux", runtime="native")
    assert info.platform == "linux"

    ops = resolve_platform_ops()
    resolved = ops.platform_info()
    assert resolved.platform in {"linux", "darwin"}
    assert ops.path_separator() in {"/", "\\"}
