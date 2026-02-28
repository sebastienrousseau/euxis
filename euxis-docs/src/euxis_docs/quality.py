"""Fast integrity checks for documentation structure."""

from __future__ import annotations

from pathlib import Path
from typing import Iterable

DEFAULT_REQUIRED_DOCS: tuple[Path, ...] = (
    Path("README.md"),
    Path("docs/index.rst"),
    Path("docs/modules/index.md"),
    Path("docs/modules/euxis-docs.md"),
)

_SUPPORTED_DOC_EXTENSIONS = {".md", ".rst", ".yaml", ".yml"}


def docs_source_files(repo_root: Path) -> list[Path]:
    """Return sorted docs files relative to *repo_root*."""
    docs_dir = repo_root / "docs"
    if not docs_dir.is_dir():
        return []

    files: list[Path] = []
    for candidate in docs_dir.rglob("*"):
        if not candidate.is_file():
            continue
        if candidate.suffix.lower() not in _SUPPORTED_DOC_EXTENSIONS:
            continue
        files.append(candidate.relative_to(repo_root))
    return sorted(files, key=str)


def missing_required_docs(
    repo_root: Path, required_docs: Iterable[Path] = DEFAULT_REQUIRED_DOCS
) -> list[Path]:
    """Return required docs that do not exist."""
    missing: list[Path] = []
    for required in required_docs:
        if not (repo_root / required).is_file():
            missing.append(required)
    return sorted(missing, key=str)


def module_doc_path(package_name: str) -> Path:
    """Map a package name to docs/modules/<package>.md."""
    normalized = package_name.strip().lower()
    if not normalized.startswith("euxis-"):
        normalized = f"euxis-{normalized}"
    return Path("docs/modules") / f"{normalized}.md"


def missing_module_docs(repo_root: Path, package_names: Iterable[str]) -> list[str]:
    """Return package names missing a module page under docs/modules."""
    missing: list[str] = []
    for package_name in sorted(set(package_names)):
        doc_path = repo_root / module_doc_path(package_name)
        if not doc_path.is_file():
            missing.append(package_name)
    return missing


def build_docs_quality_report(repo_root: Path, package_names: Iterable[str]) -> dict[str, object]:
    """Build a compact docs quality report."""
    source_files = docs_source_files(repo_root)
    missing_required = missing_required_docs(repo_root)
    missing_modules = missing_module_docs(repo_root, package_names)
    return {
        "docs_file_count": len(source_files),
        "missing_required_docs": [str(path) for path in missing_required],
        "missing_module_docs": missing_modules,
        "ok": not missing_required and not missing_modules,
    }
