#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

PYTEST=".venv/bin/pytest"
if [[ ! -x "${PYTEST}" ]]; then
  PYTEST="pytest"
fi

TIMEOUT_BIN="/usr/bin/timeout"
if [[ ! -x "${TIMEOUT_BIN}" ]]; then
  echo "missing /usr/bin/timeout; cannot enforce bounded test runtime" >&2
  exit 1
fi

echo "== euxis-identity unit tests =="
"${TIMEOUT_BIN}" -k 10 60 "${PYTEST}" -q --tb=short \
  --pythonpath=euxis-identity/src \
  euxis-identity/tests/
