from __future__ import annotations

from pathlib import Path

from euxis_docs.quality import (
    build_docs_quality_report,
    docs_source_files,
    missing_module_docs,
    missing_required_docs,
    module_doc_path,
)


def _touch(path: Path, content: str = "# test\n") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def test_docs_source_files_returns_sorted_supported_files(tmp_path: Path) -> None:
    _touch(tmp_path / "docs" / "b.rst")
    _touch(tmp_path / "docs" / "a.md")
    _touch(tmp_path / "docs" / "nested" / "openapi.yaml", "openapi: 3.1.0\n")
    _touch(tmp_path / "docs" / "ignore.txt", "skip\n")

    assert docs_source_files(tmp_path) == [
        Path("docs/a.md"),
        Path("docs/b.rst"),
        Path("docs/nested/openapi.yaml"),
    ]


def test_docs_source_files_handles_missing_docs_directory(tmp_path: Path) -> None:
    assert docs_source_files(tmp_path) == []


def test_missing_required_docs_default_and_custom(tmp_path: Path) -> None:
    _touch(tmp_path / "README.md")
    _touch(tmp_path / "docs" / "index.rst")
    _touch(tmp_path / "docs" / "modules" / "index.md")
    _touch(tmp_path / "docs" / "modules" / "euxis-docs.md")

    assert missing_required_docs(tmp_path) == []
    assert missing_required_docs(tmp_path, [Path("docs/extra.md")]) == [Path("docs/extra.md")]


def test_module_doc_path_normalizes_name() -> None:
    assert module_doc_path("core") == Path("docs/modules/euxis-core.md")
    assert module_doc_path(" euxis-cli ") == Path("docs/modules/euxis-cli.md")


def test_missing_module_docs_detects_absent_pages(tmp_path: Path) -> None:
    _touch(tmp_path / "docs" / "modules" / "euxis-core.md")
    _touch(tmp_path / "docs" / "modules" / "euxis-cli.md")

    assert missing_module_docs(tmp_path, ["core", "core", "cli", "gateway"]) == ["gateway"]


def test_build_docs_quality_report(tmp_path: Path) -> None:
    _touch(tmp_path / "README.md")
    _touch(tmp_path / "docs" / "index.rst")
    _touch(tmp_path / "docs" / "modules" / "index.md")
    _touch(tmp_path / "docs" / "modules" / "euxis-docs.md")
    _touch(tmp_path / "docs" / "modules" / "euxis-core.md")

    report = build_docs_quality_report(tmp_path, ["core", "gateway"])
    assert report == {
        "docs_file_count": 4,
        "missing_required_docs": [],
        "missing_module_docs": ["gateway"],
        "ok": False,
    }
