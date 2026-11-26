#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compose GitHub release notes in the standards' section order.

    compose_notes.py <tag> --sums <file> [--highlights <file>]

Highlights come from the `* ` lines of .github/releases/<tag>.md at the tag
(or --highlights, to rebuild the notes of a release that predates that file).
The rest is composed: What's Changed (each pull request, or commit without
one, since the previous tag), New Contributors, Checksums (every line of
--sums, "<sha256>  <asset>") and the Full Changelog link. Exit 1 on a
missing piece, so a release is never published with incomplete notes.
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
REPO = "sebastienrousseau/euxis"
OWNER = REPO.split("/", maxsplit=1)[0]
TAG = re.compile(r"^v\d+\.\d+\.\d+$")
SUM_LINE = re.compile(r"^[0-9a-f]{64}  \S")
EM_DASH = "—"


class NotesError(Exception):
    """A piece of the notes is missing or malformed."""


def run(*argv: str) -> str:
    """Run a fixed git or gh command in ROOT; return stdout or raise."""
    exe = shutil.which(argv[0]) or argv[0]
    # argv is fixed words plus a tag that TAG has validated.
    proc = subprocess.run([exe, *argv[1:]], cwd=ROOT, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise NotesError(f"{' '.join(argv)}: {proc.stderr.strip() or proc.stdout.strip()}")
    return proc.stdout


def previous_tag(tag: str, tags: list[str]) -> str | None:
    """The release tag just below `tag` in version order, if any."""
    def key(t: str) -> tuple[int, ...]:
        return tuple(int(p) for p in t[1:].split("."))

    older = sorted((t for t in tags if TAG.match(t) and key(t) < key(tag)), key=key)
    return older[-1] if older else None


def plain(title: str) -> str:
    """A title with any em dash replaced by a spaced hyphen."""
    return title.replace(f" {EM_DASH} ", " - ").replace(EM_DASH, " - ")


def entry_for(sha: str) -> tuple[str, dict[str, str]]:
    """(dedup key, entry) for one first-parent commit: its PR, else itself."""
    pulls = json.loads(run("gh", "api", f"repos/{REPO}/commits/{sha}/pulls"))
    if pulls:
        pr = pulls[0]
        return pr["html_url"], {"title": plain(pr["title"]), "author": pr["user"]["login"], "url": pr["html_url"]}
    commit = json.loads(run("gh", "api", f"repos/{REPO}/commits/{sha}"))
    author = (commit.get("author") or {}).get("login", "unknown")
    subject = commit["commit"]["message"].splitlines()[0]
    return sha, {"title": plain(subject), "author": author, "url": commit["html_url"]}


def changes(tag: str, prev: str | None) -> list[dict[str, str]]:
    """Each pull request (or commit without one) in prev..tag, oldest first."""
    span = f"{prev}..{tag}" if prev else tag
    seen: dict[str, dict[str, str]] = {}
    for sha in run("git", "rev-list", "--reverse", "--first-parent", span).split():
        key, entry = entry_for(sha)
        seen.setdefault(key, entry)
    return list(seen.values())


def new_contributors(entries: list[dict[str, str]]) -> list[dict[str, str]]:
    """First merged PR of each author (not the owner or a bot) new in this release."""
    first: list[dict[str, str]] = []
    authors = dict.fromkeys(e["author"] for e in entries if e["author"] != OWNER and "[bot]" not in e["author"])
    for author in authors:
        query = f"repo:{REPO} is:pr is:merged author:{author}"
        total = json.loads(run("gh", "api", "-X", "GET", "search/issues", "-f", f"q={query}"))["total_count"]
        mine = [e for e in entries if e["author"] == author and "/pull/" in e["url"]]
        if mine and total <= len(mine):
            first.append(mine[0])
    return first


def highlight_lines(text: str) -> str:
    """The `* ` bullet lines of a highlights file; raise if there are none."""
    bullets = [line for line in text.splitlines() if line.startswith("* ")]
    if not bullets:
        raise NotesError("the highlights file has no `* ` bullets")
    return "\n".join(bullets)


def sum_lines(text: str) -> str:
    """The checksum lines; raise unless every line is `<sha256>  <name>`."""
    lines = [line for line in text.splitlines() if line.strip()]
    bad = [line for line in lines if not SUM_LINE.match(line)]
    if not lines or bad:
        raise NotesError(f"checksums must be '<sha256>  <asset>' lines; bad: {bad[:1]}")
    return "\n".join(lines)


def compose(tag: str, prev: str | None, highlights: str, entries: list[dict[str, str]],
            newcomers: list[dict[str, str]], sums: str) -> str:
    """The release notes, in the section order of the standards (AGENTS.md section 3)."""
    lines = ["## Highlights ⭐️", "", highlights, "", "## What's Changed", ""]
    lines += [f"* {e['title']} by @{e['author']} in {e['url']}" for e in entries]
    if newcomers:
        lines += ["", "## New Contributors", ""]
        lines += [f"* @{e['author']} made their first contribution in {e['url']}" for e in newcomers]
    lines += ["", "## Checksums", "", "SHA-256 of every asset attached to this release:", "", "```text", sums, "```"]
    full = f"compare/{prev}...{tag}" if prev else f"commits/{tag}"
    lines += ["", f"**Full Changelog**: https://github.com/{REPO}/{full}", ""]
    notes = "\n".join(lines)
    if EM_DASH in notes:
        raise NotesError("the release notes contain an em dash")
    return notes


def notes_for(tag: str, sums_path: Path, highlights_path: Path | None) -> str:
    """Gather every piece for `tag` and compose its notes."""
    if not TAG.match(tag):
        raise NotesError(f"{tag}: a release tag is v<major>.<minor>.<patch>")
    if highlights_path is not None:
        raw = highlights_path.read_text(encoding="utf-8")
    else:
        raw = run("git", "show", f"{tag}:.github/releases/{tag}.md")
    prev = previous_tag(tag, run("git", "tag", "--list", "v*").split())
    entries = changes(tag, prev)
    sums = sum_lines(sums_path.read_text(encoding="utf-8"))
    return compose(tag, prev, highlight_lines(raw), entries, new_contributors(entries), sums)


def main(argv: list[str] | None = None) -> int:
    """Print the notes for a tag; report a missing piece on stderr."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tag")
    parser.add_argument("--sums", type=Path, required=True, help="'<sha256>  <asset>' lines")
    parser.add_argument("--highlights", type=Path, help="instead of .github/releases/<tag>.md")
    args = parser.parse_args(argv)
    try:
        print(notes_for(args.tag, args.sums, args.highlights), end="")
    except (NotesError, OSError) as error:
        print(f"compose_notes: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
