#!/bin/bash
# Euxis Installer
set -euo pipefail

echo "Installing Euxis Fleet..."
mkdir -p "${HOME}/bin"

echo "Linking tools to ~/bin..."
for script in "${HOME}/.euxis/bin/"*; do
    if [[ -x "${script}" ]]; then
        name="$(basename "${script}" .sh)"
        if [[ "${script}" == *.sh && -x "${HOME}/.euxis/bin/${name}" ]]; then
            continue
        fi
        ln -sf "${script}" "${HOME}/bin/${name}"
        echo "  - ${name}"
    fi
done

echo "Verifying protocol headers..."
"${HOME}/bin/euxis-health" --silent || echo "Health check flagged issues. Run 'euxis-health' for details."

echo "Euxis installed. Restart your terminal to refresh PATH."
