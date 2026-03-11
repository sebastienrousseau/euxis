#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

TIMEOUT_BIN="/usr/bin/timeout"
if [[ ! -x "${TIMEOUT_BIN}" ]]; then
  echo "missing /usr/bin/timeout; cannot enforce bounded sdk-rust test runtime" >&2
  exit 1
fi

echo "== euxis-sdk (cargo test --all-targets --all-features) =="
"${TIMEOUT_BIN}" -k 30 240 cargo test \
  --manifest-path euxis-sdk/Cargo.toml \
  --all-targets \
  --all-features
