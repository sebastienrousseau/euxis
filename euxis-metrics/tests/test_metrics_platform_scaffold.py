from metrics.platform.contracts import PlatformInfo
from metrics.platform.factory import resolve_platform_ops


def test_metrics_platform_scaffold_contract_and_factory() -> None:
    info = PlatformInfo(platform="linux", runtime="generic")
    assert info.runtime == "generic"
    ops = resolve_platform_ops()
    assert isinstance(ops.platform_info(), PlatformInfo)
    assert ops.path_separator() == "/"
