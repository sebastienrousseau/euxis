#!/usr/bin/env bash
# Prove a Linux euxis-cli is portable before it ships.
#
#   scripts/release/check-portable-linux.sh <build-dir>
#
# Runs on the GitHub runner (Docker and QEMU user mode available):
#   1. static: no program interpreter, no DT_NEEDED entries;
#   2. baseline ISA: no -march in any compile command;
#   3. old CPU: runs under QEMU as a pre-AVX x86-64 / ARMv8.0 core;
#   4. distros: runs on musl and on glibc 2.28 to current, with and
#      without /etc/ssl/cert.pem, and completes an HTTPS request with
#      certificate verification (OSV.dev) using each host's CA bundle.
set -euo pipefail

build_dir=${1:?usage: check-portable-linux.sh <build-dir>}
bin="$build_dir/apps/cli/euxis-cli"
[[ -x "$bin" ]] || { echo "error: $bin not found" >&2; exit 1; }
bin_dir=$(cd "$(dirname "$bin")" && pwd)

echo "== static"
if readelf -lW "$bin" | grep -q 'Requesting program interpreter'; then
  echo "error: $bin requests a dynamic loader" >&2; exit 1
fi
if readelf -dW "$bin" | grep -q '(NEEDED)'; then
  echo "error: $bin needs shared libraries" >&2; exit 1
fi
echo "ok: no interpreter, no shared libraries"

echo "== baseline ISA"
if grep -o -- '-march=[^ "]*' "$build_dir/compile_commands.json" | sort -u | grep .; then
  echo "error: compiled with -march (see above); expected the baseline ISA" >&2; exit 1
fi
echo "ok: no -march in compile_commands.json"

echo "== old CPU (QEMU user mode)"
case "$(uname -m)" in
  x86_64) qemu="qemu-x86_64"; cpu="qemu64" ;;       # x86-64-v1: no SSE4.2, no AVX
  aarch64) qemu="qemu-aarch64"; cpu="cortex-a53" ;; # ARMv8.0-A
  *) echo "error: unsupported host $(uname -m)" >&2; exit 1 ;;
esac
HOME=/tmp "$qemu" -cpu "$cpu" "$bin" --version
echo "ok: runs on $cpu"

echo "== distributions"
# The PURL has known advisories, so a verified TLS round trip to
# api.osv.dev returns a non-empty result.
purl='pkg:npm/lodash@4.17.20'
run() { # image, setup command
  echo "-- $1"
  docker run --rm -e HOME=/tmp -v "$bin_dir:/w:ro" "$1" sh -euc "
    $2
    /w/euxis-cli --version
    /w/euxis-cli vulndb query '$purl' --json > /tmp/osv.json
    grep -q GHSA- /tmp/osv.json
    echo 'TLS ok'
  "
}
run alpine:3.22 'apk add -q --no-cache ca-certificates'
run debian:12 'apt-get -qq update && apt-get -qq install -y ca-certificates >/dev/null'
run rockylinux:8 'true'
run rockylinux:9 'rm -f /etc/ssl/cert.pem'
run fedora:42 'rm -f /etc/ssl/cert.pem'
echo "ok: portable"
