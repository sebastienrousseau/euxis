# Legacy cleanup — 2026

This document records the directories removed from the repository
during the v0.0.3 P0 work and explains why each one is gone. Future
contributors who notice an old reference can use this as the
canonical source of truth for "where did `tui/` go?".

## Summary

The C++23 rewrite produced a small archipelago of stranded
directories — some from the pre-rewrite Python tree, some from local
tool caches that crept into a tracked path before `.gitignore` caught
up. None of them had any tracked files at the time of this commit,
so the cleanup is metadata-only: `.gitignore` is updated to make the
directories permanently invisible to git, and contributors are asked
to delete the on-disk leftovers via `./.git/cleanup-stale-dirs.sh`.

## Directories removed

| Path | What it was | Replacement |
|---|---|---|
| `tui/` | Python `Textual` TUI from the v0.1.3-era prototype | `apps/cli/src/tui/` (native C++ TUI: chat widget, diff view, syntax, event loop, keybindings, palette) |
| `.venv/` | Local Python virtualenv used while the project was Python-first | None — the C++23 binary is the only runtime now. Contributors who still want Python tooling for ancillary scripts should keep their venv outside the repo (e.g. `~/.venvs/euxis-tooling`). |
| `.venv-voice/` | Separate virtualenv from the abandoned voice-interface experiment | None — `euxis voice` is in the [aspirational](../reference/cli-reference.md) command tier; if it ships it will be a C++23 native module, not a Python overlay. |
| `bin/` | Pre-rewrite hand-rolled Python `euxis` wrapper | `apps/cli/euxis-cli` (built into `cmake-build/apps/cli/euxis-cli`, installed to `/usr/local/bin/euxis` per `make cpp-build && sudo ln -sf`). |
| `bin/__pycache__/` | CPython bytecode cache from the above | n/a |
| `metrics/` | Empty placeholder from the pre-rewrite layout | `libs/metrics/` (mdspan telemetry, fast collector, validation pipeline). |
| `bus/` | Empty placeholder for the agent-bus prototype | `libs/a2a/` (A2A v0.2 protocol — agent cards, JSON-RPC server, msgpack transport). |
| `packages/` | Empty placeholder for a packaging-output directory | `cmake-build/` (CMake's out-of-tree build dir; `cpack` artefacts land in `cmake-build/_CPack_Packages/`). |
| `prompts/` | Empty placeholder | `data/agents/prompts/` (canonical home for every agent prompt). |
| `codex/` | Stale directory from an aborted vendor experiment | None. |
| `.pytest_cache/` | Local pytest tool cache | n/a — moved to a `__pycache__/`-style global gitignore so it can't return. |

## What this commit changes

1. Adds the directories above to the top-level `.gitignore` so they
   cannot be re-introduced into the tracked tree.
2. Adds a global `__pycache__/` + `*.pyc` rule so transient Python
   build artefacts never appear again, regardless of where they
   land.
3. Records this rationale at `docs/architecture/legacy-cleanup-2026.md`.

## What this commit does NOT change

The on-disk directories are *not* deleted by this commit because none
of them were tracked in the first place — there is nothing for git to
remove. To clear them from your working tree, dry-run first:

```bash
for d in tui .venv .venv-voice bin metrics bus packages prompts codex .pytest_cache; do
    if [ -e "$d" ]; then
        tracked=$(git ls-files "$d" 2>/dev/null | wc -l | tr -d ' ')
        bytes=$(du -sk "$d" 2>/dev/null | awk '{print $1}')
        printf "  %-18s tracked=%s, %s KB\n" "$d" "$tracked" "$bytes"
    fi
done
```

Then, once you have eyes on the inventory, remove only the
directories you have no local use for:

```bash
# Example — pick the subset that applies to your checkout.
rm -rf tui bus packages prompts codex bin/__pycache__ \
       .pytest_cache .venv-voice
```

The Python virtualenvs (`.venv`, `.venv-voice`) might still be in
local use for ancillary tooling against the C++23 build, so they're
listed separately above — review them before deleting.

## Why split tracking from on-disk deletion

A bulk shell script that `rm -rf`s anonymous directories is exactly
the kind of action that wants a human in the loop. Treating the
deletion as a separate, eyes-on step lets each contributor:

- Review the list against their own checkout.
- Skip directories they want to keep locally.
- Run the cleanup once and never again.

A future CI step (planned with [`P0.12` — architecture-quality
workflow repair](../../README.md#contributing)) will assert that the
directories listed here are never re-added to a tracked path, so
that even if a stray `tui/` appears in someone's working tree, it
cannot land in a PR.
