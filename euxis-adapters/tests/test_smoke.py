# SPDX-License-Identifier: AGPL-3.0-or-later

from pathlib import Path


def test_adapters_source_layout_exists() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    assert (repo_root / "euxis-adapters" / "src" / "adapters" / "registry.py").exists()
