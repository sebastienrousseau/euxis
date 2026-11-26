#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Release preflight: block a tag that breaks the standards' release invariants.

    preflight.py <tag> [--remote] [--verify-signature] [--legacy]

Checks, at the tag: it is annotated and signed, its message is
"euxis <tag>", it points at a commit on origin/main, CMakeLists.txt
declares the tag's version, CHANGELOG.md has "## [<version>] - YYYY-MM-DD"
and .github/releases/<tag>.md has highlight bullets. --verify-signature
also checks the signature with git's allowed signers; --remote requires
zero open Dependabot alerts and zero open issues. --legacy skips the
CHANGELOG and highlights checks, for a tag on a commit that predates them
(its notes are then composed with compose_notes.py --highlights).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(os.environ.get("EUXIS_REPO") or Path(__file__).resolve().parents[2])
PROJECT = "euxis"
REPO = "sebastienrousseau/euxis"
TAG = re.compile(r"^v(\d+\.\d+\.\d+)$")
SIGNATURE = re.compile(r"^-----BEGIN (SSH|PGP) SIGNATURE-----$", re.MULTILINE)


class PreflightError(Exception):
    """A release-blocking failure."""


def run(*argv: str) -> str:
    """Run a fixed git or gh command in ROOT; return stdout or raise."""
    exe = shutil.which(argv[0]) or argv[0]
    # argv is fixed words plus a tag that TAG has validated.
    proc = subprocess.run([exe, *argv[1:]], cwd=ROOT, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise PreflightError(f"{' '.join(argv)}: {proc.stderr.strip() or proc.stdout.strip()}")
    return proc.stdout


def version_of(tag: str) -> str:
    """`v0.0.17` -> `0.0.17`; raise on any other shape."""
    match = TAG.match(tag)
    if not match:
        raise PreflightError(f"{tag}: a release tag is v<major>.<minor>.<patch>")
    return match.group(1)


def check_tag_object(tag: str, kind: str, raw: str, subject: str) -> None:
    """Annotated, carries a signature, and its message is `euxis <tag>`."""
    if kind != "tag":
        raise PreflightError(f"{tag} is a lightweight tag; release tags are annotated")
    if not SIGNATURE.search(raw):
        raise PreflightError(f"{tag} is not signed")
    if subject != f"{PROJECT} {tag}":
        raise PreflightError(f"{tag} message is {subject!r}; it must be {PROJECT + ' ' + tag!r}")


def check_files(tag: str, cmake: str, changelog: str | None, highlights: str | None) -> None:
    """Every version carrier at the tag names the tag's version."""
    version = re.escape(version_of(tag))
    if not re.search(rf"(?m)^project\(euxis VERSION {version}\b", cmake):
        raise PreflightError(f"CMakeLists.txt at {tag} does not declare version {version_of(tag)}")
    if changelog is not None and not re.search(rf"(?m)^## \[{version}\] - \d{{4}}-\d{{2}}-\d{{2}}$", changelog):
        raise PreflightError(f"CHANGELOG.md at {tag} has no '## [{version_of(tag)}] - YYYY-MM-DD' section")
    if highlights is not None and not re.search(r"(?m)^\* \*\*", highlights):
        raise PreflightError(f".github/releases/{tag}.md at {tag} has no highlight bullets")


def check_remote(alerts: list[object], issues: list[object]) -> None:
    """Zero open Dependabot alerts and zero open issues (standards section 0)."""
    if alerts or issues:
        raise PreflightError(f"{len(alerts)} open Dependabot alert(s) and {len(issues)} open issue(s)")


def preflight(tag: str, *, remote: bool, verify: bool, legacy: bool) -> None:
    """Gather each piece with git/gh and run every check."""
    version_of(tag)
    check_tag_object(tag, run("git", "cat-file", "-t", tag).strip(), run("git", "cat-file", "tag", tag),
                     run("git", "tag", "-l", "--format=%(contents:subject)", tag).strip())
    if verify:
        run("git", "tag", "-v", tag)
    run("git", "merge-base", "--is-ancestor", f"{tag}^{{commit}}", "origin/main")
    changelog = None if legacy else run("git", "show", f"{tag}:CHANGELOG.md")
    highlights = None if legacy else run("git", "show", f"{tag}:.github/releases/{tag}.md")
    check_files(tag, run("git", "show", f"{tag}:CMakeLists.txt"), changelog, highlights)
    if remote:
        alerts = json.loads(run("gh", "api", f"repos/{REPO}/dependabot/alerts?state=open&per_page=100"))
        issues = json.loads(run("gh", "issue", "list", "--repo", REPO, "--state", "open", "--json", "number"))
        check_remote(alerts, issues)


def main(argv: list[str] | None = None) -> int:
    """Run the preflight; report a blocking failure on stderr."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tag")
    parser.add_argument("--remote", action="store_true", help="also require zero open alerts and issues")
    parser.add_argument("--verify-signature", action="store_true", help="git tag -v with the allowed signers")
    parser.add_argument("--legacy", action="store_true", help="skip the CHANGELOG and highlights checks")
    args = parser.parse_args(argv)
    try:
        preflight(args.tag, remote=args.remote, verify=args.verify_signature, legacy=args.legacy)
    except PreflightError as error:
        print(f"preflight: {error}", file=sys.stderr)
        return 1
    print(f"preflight: {args.tag} is ready to release")
    return 0


if __name__ == "__main__":
    sys.exit(main())
