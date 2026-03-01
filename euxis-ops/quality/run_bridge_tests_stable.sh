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
  echo "missing /usr/bin/timeout; cannot enforce bounded bridge test runtime" >&2
  exit 1
fi

required_files=(
  "README_BRIDGE.md"
  "bridge_config.yaml"
  "euxis-ops/bridge/daemon.py"
  "euxis-ops/bridge/import_openclaw.py"
  "euxis-ops/bridge/signed_exec.py"
  "euxis-ops/bridge/signature_tools.py"
  "euxis-ops/bridge/openclaw_skill_map.yaml"
  "euxis-ops/bridge/system_prompt_openclaw_bridge.txt"
)
for file in "${required_files[@]}"; do
  [[ -f "${file}" ]] || { echo "missing bridge artifact: ${file}" >&2; exit 1; }
done

echo "== euxis-core/tests/unit/test_bridge_tools.py =="
"${TIMEOUT_BIN}" -k 10 60 "${PYTEST}" -q -c /dev/null -p no:cacheprovider euxis-core/tests/unit/test_bridge_tools.py
