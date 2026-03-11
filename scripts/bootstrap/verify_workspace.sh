#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

python3 euxis-ops/templates/apply_template_overlay.py --repo-root . --standards euxis-ops/quality/package_standards.json
python3 euxis-ops/templates/enforce_package_structure.py --repo-root . --standards euxis-ops/quality/package_standards.json --json-output data/release/structure-enforcement.json
python3 euxis-ops/quality/validate_template_conformance.py --repo-root . --standards euxis-ops/quality/package_standards.json --json-output data/release/template-conformance.json
python3 euxis-ops/quality/generate_split_readiness_report.py --repo-root . --standards euxis-ops/quality/package_standards.json --json-output data/release/repo-split-readiness.json --md-output data/release/repo-split-readiness.md
make verify-all-packages REQUIRE_DEPS=1
