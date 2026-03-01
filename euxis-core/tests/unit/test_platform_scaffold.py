from euxis_core.platform.contracts import PlatformInfo
from euxis_core.platform.factory import resolve_platform_ops


def test_core_platform_scaffold_contract_and_factory() -> None:
    info = PlatformInfo(platform="linux", runtime="generic")
    assert info.platform == "linux"
    assert info.runtime == "generic"
    ops = resolve_platform_ops()
    resolved = ops.platform_info()
    assert isinstance(resolved, PlatformInfo)
    assert ops.path_separator() == "/"
