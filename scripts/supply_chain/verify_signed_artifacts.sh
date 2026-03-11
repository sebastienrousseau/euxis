#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later

set -euo pipefail

ARTIFACT_DIR="${1:-release-artifacts}"

if ! command -v cosign >/dev/null 2>&1; then
  echo "cosign is required but not installed." >&2
  exit 2
fi

if [[ ! -d "${ARTIFACT_DIR}" ]]; then
  echo "Artifact directory not found: ${ARTIFACT_DIR}" >&2
  exit 2
fi

mapfile -t artifacts < <(find "${ARTIFACT_DIR}" -type f \( -name '*.whl' -o -name '*.tar.gz' \) | sort)

if [[ "${#artifacts[@]}" -eq 0 ]]; then
  echo "No wheel or sdist artifacts found in ${ARTIFACT_DIR}" >&2
  exit 1
fi

for artifact in "${artifacts[@]}"; do
  cert="${artifact}.pem"
  sig="${artifact}.sig"
  if [[ ! -f "${cert}" || ! -f "${sig}" ]]; then
    echo "Missing signature or certificate for ${artifact}" >&2
    exit 1
  fi
  cosign verify-blob \
    --certificate "${cert}" \
    --signature "${sig}" \
    --certificate-oidc-issuer https://token.actions.githubusercontent.com \
    --certificate-identity-regexp "https://github.com/.+" \
    "${artifact}" >/dev/null
  echo "Verified: ${artifact}"
done

echo "All signed artifacts verified."
