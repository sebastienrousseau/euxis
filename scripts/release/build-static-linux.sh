#!/bin/sh
# Build a fully static, musl-linked euxis-cli for distribution.
#
# Runs inside Alpine (see EUXIS_ALPINE_IMAGE in release.yml and cpp.yml):
#
#   docker run --rm -v "$PWD:/src" -w /src \
#     -e EUXIS_UID="$(id -u)" -e EUXIS_GID="$(id -g)" "$EUXIS_ALPINE_IMAGE" \
#     scripts/release/build-static-linux.sh [build-dir] [--with-tests]
#
# The binary needs no shared library from the host (no glibc, libstdc++,
# OpenSSL, SQLite or libsodium) and targets the baseline ISA (x86-64 or
# armv8-a), so it runs on any Linux distribution and CPU of its arch.
# --with-tests also builds and runs the full ctest suite under musl.
#
# Only package installation runs as root. With EUXIS_UID/EUXIS_GID set,
# the build and the tests run as that user: the tree stays owned by the
# caller, and permission tests are not bypassed by root.
set -eu

build_dir=${1:-cmake-build}
with_tests=${2:-}

apk add --no-cache \
  build-base cmake samurai git linux-headers pkgconf perl python3 file \
  setpriv \
  openssl-dev openssl-libs-static zlib-dev zlib-static \
  sqlite-dev sqlite-static libsodium-dev libsodium-static >/dev/null

uid=${EUXIS_UID:-0}
gid=${EUXIS_GID:-0}
as_user() {
  if [ "$uid" = 0 ]; then
    "$@"
  else
    HOME=/tmp setpriv --reuid="$uid" --regid="$gid" --clear-groups -- "$@"
  fi
}
# Root (uid 0) does not own the runner's checkout.
git config --system --add safe.directory "$PWD"

as_user cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEUXIS_DISABLE_SANITIZERS=ON \
  -DEUXIS_NATIVE_ARCH=OFF \
  -DEUXIS_PORTABLE_ARCH=ON \
  -DEUXIS_STATIC_BINARY=ON

# Default 2 jobs: Release+LTO translation units peak at ~3 GB each,
# which is the limit on the 16 GB arm64 runner.
jobs=${EUXIS_BUILD_JOBS:-2}
if [ "$with_tests" = --with-tests ]; then
  as_user cmake --build "$build_dir" --parallel "$jobs"
  as_user ctest --test-dir "$build_dir" --output-on-failure --parallel "$jobs"
else
  as_user cmake --build "$build_dir" --parallel "$jobs" --target euxis-cli
fi

bin="$build_dir/apps/cli/euxis-cli"
file "$bin"
# A static-pie has no PT_INTERP and no DT_NEEDED entries.
if readelf -lW "$bin" | grep -q 'Requesting program interpreter'; then
  echo "error: $bin requests a dynamic loader" >&2
  exit 1
fi
if readelf -dW "$bin" | grep -q '(NEEDED)'; then
  echo "error: $bin depends on shared libraries:" >&2
  readelf -dW "$bin" | grep '(NEEDED)' >&2
  exit 1
fi
HOME=/tmp "$bin" --version
