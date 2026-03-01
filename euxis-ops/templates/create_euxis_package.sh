#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <euxis-package-name> [target-dir]" >&2
  exit 2
fi

pkg_name="$1"
target_dir="${2:-$1}"

template_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/euxis-package-template"

if [[ ! -d "${template_dir}" ]]; then
  echo "missing template directory: ${template_dir}" >&2
  exit 1
fi

if [[ ! "${pkg_name}" =~ ^euxis-[a-z0-9]+$ ]]; then
  echo "invalid package name: ${pkg_name} (expected euxis-<oneword>)" >&2
  exit 1
fi

if [[ -e "${target_dir}" ]]; then
  echo "target already exists: ${target_dir}" >&2
  exit 1
fi

slug="${pkg_name#euxis-}"
module_name="euxis_${slug}"

cp -R "${template_dir}" "${target_dir}"
mv "${target_dir}/src/euxis_template" "${target_dir}/src/${module_name}"

while IFS= read -r f; do
  perl -0pi -e "s/euxis-template/${pkg_name}/g; s/euxis_template/${module_name}/g; s/euxis-<package>/${pkg_name}/g" "$f"
done < <(find "${target_dir}" -type f)

echo "created package scaffold at ${target_dir}"
echo "next:"
echo "  1) add ${pkg_name} into euxis-ops/quality/package_standards.json"
echo "  2) add module doc page under euxis-data/docs/modules/"
echo "  3) wire stable tests script + CI workflow if needed"
