#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

PYTEST=".venv/bin/pytest"
if [[ ! -x "${PYTEST}" ]]; then
  PYTEST="pytest"
fi

PYTHON_BIN="python3"
if [[ "${PYTEST}" == ".venv/bin/pytest" && -x ".venv/bin/python3" ]]; then
  PYTHON_BIN=".venv/bin/python3"
fi

if ! "${PYTHON_BIN}" -c "import textual" >/dev/null 2>&1; then
  if [[ "${REQUIRE_DEPS:-0}" == "1" ]]; then
    echo "FAILED: euxis-tui stable suite requires 'textual' (missing dependency with REQUIRE_DEPS=1)" >&2
    exit 1
  fi
  echo "SKIP-ONLY: euxis-tui stable suite requires 'textual' (dependency unavailable in current offline environment)"
  exit 0
fi

TIMEOUT_BIN="/usr/bin/timeout"
if [[ ! -x "${TIMEOUT_BIN}" ]]; then
  echo "missing /usr/bin/timeout; cannot enforce bounded tui test runtime" >&2
  exit 1
fi

status=0
while IFS= read -r test_file; do
  echo "== ${test_file} =="
  # TUI test files include larger UI/state matrices; keep a wider bound.
  if "${TIMEOUT_BIN}" -k 10 120 "${PYTEST}" -q -c /dev/null -p no:cacheprovider "${test_file}"; then
    :
  else
    status=$?
    if [[ ${status} -eq 5 ]]; then
      echo "SKIP-ONLY: ${test_file} (pytest exit 5)"
      status=0
      continue
    fi
    echo "FAILED: ${test_file} (exit ${status})" >&2
    break
  fi
done < <(find euxis-tui/tests -maxdepth 2 -type f -name 'test_*.py' | sort)

exit "${status}"
