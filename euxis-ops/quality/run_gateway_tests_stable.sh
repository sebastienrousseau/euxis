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
  echo "missing /usr/bin/timeout; cannot enforce bounded gateway test runtime" >&2
  exit 1
fi

status=0
for test_file in euxis-gateway/tests/test_*.py; do
  echo "== ${test_file} =="
  if "${TIMEOUT_BIN}" -k 10 60 "${PYTEST}" -q -c /dev/null -p no:cacheprovider \
    -o markers="e2e: end-to-end integration tests" \
    "${test_file}"; then
    :
  else
    status=$?
    # pytest returns 5 when no tests are collected (expected for stub-only files).
    if [[ ${status} -eq 5 ]]; then
      echo "SKIP-ONLY: ${test_file} (pytest exit 5)"
      status=0
      continue
    fi
    echo "FAILED: ${test_file} (exit ${status})" >&2
    break
  fi
done

if [[ ${status} -eq 0 ]]; then
  echo "== euxis-gateway/tests (coverage gate) =="
  pushd euxis-gateway >/dev/null
  PYTEST_LOCAL="${PYTEST}"
  if [[ "${PYTEST_LOCAL}" == ".venv/bin/pytest" ]]; then
    PYTEST_LOCAL="../.venv/bin/pytest"
  fi
  if PYTHONPATH="src${PYTHONPATH:+:${PYTHONPATH}}" \
    "${TIMEOUT_BIN}" -k 20 180 "${PYTEST_LOCAL}" -q \
    -c pyproject.toml \
    -p no:cacheprovider \
    -o markers="e2e: end-to-end integration tests" \
    tests; then
    gate_rc=0
  else
    gate_rc=$?
  fi
  popd >/dev/null
  if [[ ${gate_rc} -ne 0 ]]; then
    status=${gate_rc}
    echo "FAILED: euxis-gateway consolidated coverage gate (exit ${status})" >&2
  fi
fi

exit "${status}"
