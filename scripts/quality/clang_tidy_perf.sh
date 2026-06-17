#!/usr/bin/env bash
set -euo pipefail
#
# scripts/quality/clang_tidy_perf.sh
#
# Driver for issue #57 — runs clang-tidy scoped to the single
# `performance-unnecessary-copy-initialization` check across every
# .cpp under apps/ + libs/, against the existing
# build/cmake-build/compile_commands.json.
#
# Two modes:
#   survey   — read-only; prints every flagged site to stdout.
#   fix      — applies clang-tidy's mechanical --fix to those sites
#              in place, then prints `git diff --stat` so you can
#              review before committing.
#
# Usage:
#   scripts/quality/clang_tidy_perf.sh survey
#   scripts/quality/clang_tidy_perf.sh fix
#
# Requires:
#   - clang-tidy in $PATH (LLVM 17 or newer recommended)
#   - build/cmake-build/compile_commands.json (run `make cpp-configure`
#     first)
#
# Policy:
#   See docs/development/clang-tidy-policy.md §2 for the patterns
#   clang-tidy WILL flag here and the patterns it WILL NOT
#   (false-positive review notes). Do not bypass the policy doc
#   when reviewing the fix diff — pattern (3) sites should not be
#   "fixed" even if the tool offers a suggestion.

cd "$(git rev-parse --show-toplevel)"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "ERROR: clang-tidy not in PATH" >&2
    echo "       Install via 'brew install llvm' (macOS) or your" >&2
    echo "       distro's clang-tools-extra package." >&2
    exit 2
fi

COMPILE_DB="build/cmake-build/compile_commands.json"
if [ ! -f "${COMPILE_DB}" ]; then
    echo "ERROR: ${COMPILE_DB} missing. Run 'make cpp-configure' first." >&2
    exit 2
fi

MODE="${1:-survey}"

# Restrict the check list to the single perf check we're fixing.
# Every other check stays at its .clang-tidy default so we don't
# accidentally apply unrelated --fix actions.
CHECKS='-*,performance-unnecessary-copy-initialization'

# Files to scan: every .cpp under apps/ and libs/, minus the build
# tree. The same query the existing `cpp-clang-tidy` Makefile target
# uses, kept in sync deliberately.
FILES=$(find apps/ libs/ -name '*.cpp' | grep -v 'build/')
if [ -z "${FILES}" ]; then
    echo "ERROR: no .cpp files found under apps/ + libs/" >&2
    exit 2
fi

case "${MODE}" in
    survey)
        echo "===== clang-tidy ${CHECKS} (read-only survey) ====="
        # shellcheck disable=SC2086
        echo "${FILES}" | xargs -P4 clang-tidy \
            -p build/cmake-build \
            --checks="${CHECKS}" \
            --quiet \
            2>/dev/null | grep -E 'warning:|error:' || {
            echo
            echo "No performance-unnecessary-copy-initialization sites flagged."
            echo "Issue #57 may already be closed."
            exit 0
        }
        ;;
    fix)
        echo "===== clang-tidy ${CHECKS} --fix (in-place rewrite) ====="
        echo "WARNING: this WILL modify source files. Commit any pending"
        echo "         work before continuing. Press Ctrl-C within 3s to abort."
        sleep 3
        # shellcheck disable=SC2086
        echo "${FILES}" | xargs -P4 clang-tidy \
            -p build/cmake-build \
            --checks="${CHECKS}" \
            --fix \
            --fix-errors \
            --quiet \
            2>/dev/null || true
        echo
        echo "===== Resulting diff stat ====="
        git diff --stat
        echo
        echo "===== Next steps ====="
        echo "  1. Review the diff against docs/development/clang-tidy-policy.md §2."
        echo "     Revert any pattern-(3) sites (enum / size_t / string_view /"
        echo "     intentional mutation) that the tool flagged incorrectly."
        echo "  2. Re-build and re-test:    make cpp-build && make cpp-test"
        echo "  3. Commit with trailer:     Closes #57"
        ;;
    *)
        echo "ERROR: unknown mode '${MODE}'. Use 'survey' or 'fix'." >&2
        exit 2
        ;;
esac
