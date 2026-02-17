#!/usr/bin/env python3
from pathlib import Path
import sys

import json
import time

import requests


def _ensure_repo_paths() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    module_src_dirs = [
        "api/src",
        "adapters/src",
        "cli/src",
        "core/src",
        "crypto/src",
        "metrics/src",
        "ui/src",
        "packages/shared/src",
    ]
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    for rel in module_src_dirs:
        path = repo_root / rel
        if path.exists():
            sys.path.insert(0, str(path))


_ensure_repo_paths()

# Copyleft licenses that may be problematic in permissive projects
COPYLEFT_LICENSES = {
    "GPL", "GPL-2.0", "GPL-3.0", "AGPL", "AGPL-3.0", "LGPL", "LGPL-2.1", "LGPL-3.0",
    "GNU GPL", "GNU GENERAL PUBLIC LICENSE", "GNU AFFERO GENERAL PUBLIC LICENSE",
    "GNU Lesser General Public License", "GNU LGPL"
}

# License normalization mapping
LICENSE_ALIASES = {
    "BSD": "BSD License",
    "MIT": "MIT License",
    "Apache": "Apache License",
    "Apache 2.0": "Apache Software License",
    "Apache-2.0": "Apache Software License",
    "ISC": "ISC License",
    "MPL": "Mozilla Public License"
}

def get_package_license(package_name: str, version: str) -> dict | None:
    """Get package license info from PyPI API."""
    try:
        clean_name = package_name.strip()
        url = f"https://pypi.org/pypi/{clean_name}/json"
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            data = response.json()
            info = data.get("info", {})

            return {
                "name": clean_name,
                "version": version,
                "license": info.get("license", ""),
                "license_expression": info.get("license_expression", ""),
                "classifiers": [c for c in info.get("classifiers", []) if "License" in c],
                "author": info.get("author", ""),
                "home_page": info.get("home_page", ""),
                "project_url": info.get("project_url", "")
            }
        return None
    except Exception:
        return None

def classify_license(license_info: dict) -> str:
    """Classify license as permissive, copyleft, or unknown."""
    all_text = f"{license_info['license']} {license_info['license_expression']} {' '.join(license_info['classifiers'])}"
    all_text_upper = all_text.upper()

    # Check for copyleft licenses
    for copyleft in COPYLEFT_LICENSES:
        if copyleft.upper() in all_text_upper:
            return "COPYLEFT"

    # Check for common permissive licenses
    permissive_indicators = ["MIT", "BSD", "APACHE", "ISC", "PYTHON SOFTWARE FOUNDATION", "UNLICENSE", "PUBLIC DOMAIN"]
    for permissive in permissive_indicators:
        if permissive in all_text_upper:
            return "PERMISSIVE"

    # If we can't determine, mark as unknown
    if not all_text.strip():
        return "UNKNOWN"

    return "OTHER"

def parse_requirements_file(file_path: str) -> list[tuple]:
    """Parse requirements-lock.txt and extract package names and versions."""
    packages = []
    try:
        with open(file_path) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "==" in line:
                    name, version = line.split("==", 1)
                    packages.append((name.strip(), version.strip()))
    except Exception:
        pass
    return packages

def check_licenses():
    """Check licenses of all packages."""
    packages = parse_requirements_file("requirements-lock.txt")

    results = {
        "permissive": [],
        "copyleft": [],
        "other": [],
        "unknown": [],
        "errors": []
    }

    for i, (name, version) in enumerate(packages):
        if i > 0:
            time.sleep(0.5)  # Rate limit PyPI API calls

        license_info = get_package_license(name, version)
        if not license_info:
            results["errors"].append({"name": name, "version": version})
            continue

        classification = classify_license(license_info)
        license_info["classification"] = classification

        if classification == "COPYLEFT":
            results["copyleft"].append(license_info)
            license_info["license"] or ""
        elif classification == "PERMISSIVE":
            results["permissive"].append(license_info)
            license_info["license"] or ""
        elif classification == "UNKNOWN":
            results["unknown"].append(license_info)
        else:
            results["other"].append(license_info)
            license_info["license"] or ""

    return results

if __name__ == "__main__":
    results = check_licenses()


    if results["copyleft"]:
        for _pkg in results["copyleft"]:
            pass

    if results["unknown"]:
        for _pkg in results["unknown"]:
            pass

    # Save full results to JSON
    with open("license_report.json", "w") as f:
        json.dump(results, f, indent=2, default=str)
