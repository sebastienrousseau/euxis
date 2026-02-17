#!/usr/bin/env python3
from pathlib import Path
import sys

import json
from datetime import datetime, timedelta

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


def get_package_info(package_name: str, version: str) -> dict | None:
    """Get package info from PyPI API."""
    try:
        # Remove version specifiers like ==, >=, etc.
        clean_name = package_name.strip()
        url = f"https://pypi.org/pypi/{clean_name}/json"
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            return response.json()
        return None
    except Exception:
        return None

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

def check_maintenance_status():
    """Check maintenance status of all packages."""
    packages = parse_requirements_file("requirements-lock.txt")

    results = {
        "maintained": [],
        "stale": [],
        "unmaintained": [],
        "errors": []
    }

    cutoff_date = datetime.now() - timedelta(days=365)  # 12 months ago
    stale_cutoff = datetime.now() - timedelta(days=180)  # 6 months ago

    for _i, (name, version) in enumerate(packages):

        info = get_package_info(name, version)
        if not info:
            results["errors"].append({"name": name, "version": version})
            continue

        try:
            # Get all releases and find the latest one
            releases = info["releases"]
            if not releases:
                results["unmaintained"].append({
                    "name": name,
                    "version": version,
                    "last_release": "No releases found"
                })
                continue

            # Find the most recent release date
            latest_date = None
            latest_version = None

            for rel_version, release_data in releases.items():
                if release_data:  # Check if release has data
                    for release in release_data:
                        if "upload_time" in release:
                            upload_time = datetime.fromisoformat(release["upload_time"].replace("Z", "+00:00"))
                            if latest_date is None or upload_time > latest_date:
                                latest_date = upload_time
                                latest_version = rel_version

            if latest_date is None:
                results["unmaintained"].append({
                    "name": name,
                    "version": version,
                    "last_release": "No upload dates found"
                })
                continue

            days_since_update = (datetime.now(latest_date.tzinfo) - latest_date).days

            package_result = {
                "name": name,
                "version": version,
                "latest_version": latest_version,
                "last_release": latest_date.strftime("%Y-%m-%d"),
                "days_since_update": days_since_update
            }

            if latest_date < cutoff_date:
                results["unmaintained"].append(package_result)
            elif latest_date < stale_cutoff:
                results["stale"].append(package_result)
            else:
                results["maintained"].append(package_result)

        except Exception as e:
            results["errors"].append({"name": name, "version": version, "error": str(e)})

    return results

if __name__ == "__main__":
    results = check_maintenance_status()


    if results["unmaintained"]:
        for _pkg in results["unmaintained"]:
            pass

    if results["stale"]:
        for _pkg in results["stale"]:
            pass

    # Save full results to JSON
    with open("package_maintenance_report.json", "w") as f:
        json.dump(results, f, indent=2)
