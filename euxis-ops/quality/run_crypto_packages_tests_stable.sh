#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

TIMEOUT_BIN="/usr/bin/timeout"
if [[ ! -x "${TIMEOUT_BIN}" ]]; then
  echo "missing /usr/bin/timeout; cannot enforce bounded crypto-packages test runtime" >&2
  exit 1
fi

echo "== euxis-web (workspace tests) =="
"${TIMEOUT_BIN}" -k 30 240 npm --prefix euxis-web test

echo "== euxis-web/src/crypto-lib (coverage gate) =="
"${TIMEOUT_BIN}" -k 30 360 npm --prefix euxis-web/src/crypto-lib run test:ci

echo "== euxis-web/src/crypto-lib (critical coverage thresholds) =="
node <<'NODE'
const fs = require('fs');

const coveragePath = 'euxis-web/src/crypto-lib/coverage/coverage-final.json';
if (!fs.existsSync(coveragePath)) {
  console.error(`missing coverage artifact: ${coveragePath}`);
  process.exit(1);
}

const coverage = JSON.parse(fs.readFileSync(coveragePath, 'utf8'));
const criticalFiles = [
  ['src/index.ts', 95],
  ['src/types.ts', 95],
  ['src/config/security.ts', 95],
  ['src/lib/crypto-core.ts', 95],
  ['src/lib/encryption.ts', 95],
];

let failed = false;
for (const [suffix, threshold] of criticalFiles) {
  const file = Object.keys(coverage).find((key) => key.endsWith(suffix));
  if (!file) {
    console.error(`missing coverage entry for ${suffix}`);
    failed = true;
    continue;
  }
  const statements = Object.values(coverage[file].s);
  const total = statements.length;
  const covered = statements.filter((count) => Number(count) > 0).length;
  const percent = total === 0 ? 100 : (covered / total) * 100;
  console.log(`${suffix}: ${percent.toFixed(2)}% statement coverage`);
  if (percent < threshold) {
    console.error(
      `coverage threshold failure for ${suffix}: ${percent.toFixed(2)}% < ${threshold}%`
    );
    failed = true;
  }
}

if (failed) {
  process.exit(1);
}
NODE
