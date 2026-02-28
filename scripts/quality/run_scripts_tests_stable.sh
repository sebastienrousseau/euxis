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
  echo "missing /usr/bin/timeout; cannot enforce bounded scripts test runtime" >&2
  exit 1
fi

status=0
while IFS= read -r test_file; do
  echo "== ${test_file} =="
  if "${TIMEOUT_BIN}" -k 10 60 "${PYTEST}" -q -c /dev/null -p no:cacheprovider "${test_file}"; then
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
done < <(find euxis-scripts/tests -maxdepth 2 -type f -name 'test_*.py' | sort)

if [[ ${status} -eq 0 ]]; then
  echo "== euxis-scripts/tests (coverage gate) =="
  pushd euxis-scripts >/dev/null
  PYTEST_LOCAL="${PYTEST}"
  if [[ "${PYTEST_LOCAL}" == ".venv/bin/pytest" ]]; then
    PYTEST_LOCAL="../.venv/bin/pytest"
  fi
  if PYTHONPATH=".${PYTHONPATH:+:${PYTHONPATH}}" \
    "${TIMEOUT_BIN}" -k 20 120 "${PYTEST_LOCAL}" -q \
    -c pyproject.toml \
    -p no:cacheprovider \
    tests; then
    gate_rc=0
  else
    gate_rc=$?
  fi
  popd >/dev/null
  if [[ ${gate_rc} -ne 0 ]]; then
    status=${gate_rc}
    echo "FAILED: euxis-scripts consolidated coverage gate (exit ${status})" >&2
  fi
fi

exit "${status}"

