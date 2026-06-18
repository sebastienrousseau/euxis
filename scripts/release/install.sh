#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-only
#
# One-line installer for the euxis CLI.
#
#   curl -fsSL https://raw.githubusercontent.com/sebastienrousseau/euxis/main/scripts/release/install.sh | sh
#
# Or to pin a specific version:
#
#   curl -fsSL https://.../install.sh | EUXIS_VERSION=v0.1.3 sh
#
# What it does:
#   1. Detects the platform (linux-amd64 / linux-arm64 / darwin-amd64 / darwin-arm64)
#   2. Downloads the matching tarball from the GitHub Release
#   3. Verifies the SHA-256 against the published checksum file
#   4. Optionally verifies the cosign keyless signature if `cosign` is on PATH
#   5. Drops the `euxis` binary into $EUXIS_INSTALL_DIR (default ~/.local/bin)
#
# Environment knobs:
#   EUXIS_VERSION       — tag to install (default: latest)
#   EUXIS_INSTALL_DIR   — install destination (default: ~/.local/bin)
#   EUXIS_NO_VERIFY     — set to 1 to skip cosign verification even if available

set -eu

REPO="sebastienrousseau/euxis"
EUXIS_VERSION="${EUXIS_VERSION:-latest}"
EUXIS_INSTALL_DIR="${EUXIS_INSTALL_DIR:-$HOME/.local/bin}"

die() {
    printf 'euxis-install: %s\n' "$*" >&2
    exit 1
}

detect_target() {
    os="$(uname -s | tr '[:upper:]' '[:lower:]')"
    case "$os" in
        linux)  os_part="linux"  ;;
        darwin) os_part="darwin" ;;
        *)      die "unsupported OS: $os (linux + darwin only)" ;;
    esac
    arch="$(uname -m)"
    case "$arch" in
        x86_64|amd64)  arch_part="amd64" ;;
        aarch64|arm64) arch_part="arm64" ;;
        *)             die "unsupported architecture: $arch" ;;
    esac
    printf '%s-%s' "$os_part" "$arch_part"
}

resolve_version() {
    if [ "$EUXIS_VERSION" = "latest" ]; then
        # GitHub redirects /releases/latest to the latest tag.
        latest_url="$(curl -fsSI "https://github.com/${REPO}/releases/latest" \
            | grep -i '^location:' | tail -1 | tr -d '\r')"
        EUXIS_VERSION="${latest_url##*/}"
        [ -n "$EUXIS_VERSION" ] || die "could not resolve latest release"
    fi
}

main() {
    target="$(detect_target)"
    resolve_version
    archive="euxis-${EUXIS_VERSION}-${target}.tar.gz"
    base="https://github.com/${REPO}/releases/download/${EUXIS_VERSION}"

    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT

    printf 'euxis-install: downloading %s\n' "$archive"
    curl -fsSL -o "$tmp/$archive"        "$base/$archive"
    curl -fsSL -o "$tmp/$archive.sha256" "$base/$archive.sha256"

    printf 'euxis-install: verifying sha256\n'
    (cd "$tmp" && shasum -a 256 -c "$archive.sha256") \
        || die "sha256 check failed"

    if [ -z "${EUXIS_NO_VERIFY:-}" ] && command -v cosign >/dev/null 2>&1; then
        printf 'euxis-install: verifying cosign signature\n'
        curl -fsSL -o "$tmp/$archive.cosign.bundle" \
            "$base/$archive.cosign.bundle"
        cosign verify-blob --bundle "$tmp/$archive.cosign.bundle" \
            --certificate-identity-regexp "https://github.com/${REPO}/" \
            --certificate-oidc-issuer https://token.actions.githubusercontent.com \
            "$tmp/$archive" \
            || die "cosign verification failed"
    fi

    printf 'euxis-install: extracting + installing to %s\n' "$EUXIS_INSTALL_DIR"
    mkdir -p "$EUXIS_INSTALL_DIR"
    (cd "$tmp" && tar -xzf "$archive")
    bin="euxis-${EUXIS_VERSION}-${target}"
    install -m 0755 "$tmp/$bin" "$EUXIS_INSTALL_DIR/euxis"

    printf 'euxis-install: done — %s\n' "$EUXIS_INSTALL_DIR/euxis"
    case ":$PATH:" in
        *":$EUXIS_INSTALL_DIR:"*) ;;
        *) printf 'euxis-install: NOTE %s is not on $PATH\n' "$EUXIS_INSTALL_DIR" ;;
    esac
}

main "$@"
