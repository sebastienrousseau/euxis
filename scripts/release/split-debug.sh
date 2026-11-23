#!/usr/bin/env bash
# Split the debug info out of a release binary, then strip it in place.
#
#   scripts/release/split-debug.sh <binary> <name>
#
# <name> is the name the binary ships under (euxis-v1.2.3-linux-amd64).
# Writes next to <binary>:
#   Linux  <name>.debug  ELF debug file, found by debuggers through the
#                        .gnu_debuglink section or the GNU build ID
#   macOS  <name>.dSYM   dSYM bundle, matched to the binary by its UUID
#
# Fails unless the stripped binary has no symbol table or debug
# sections, the debug file has line tables, and both carry the same
# build ID / UUID. Runs natively on the build host (bash 3.2 compatible).
set -euo pipefail

bin=${1:?usage: split-debug.sh <binary> <name>}
name=${2:?usage: split-debug.sh <binary> <name>}
[[ -f "$bin" ]] || { echo "error: $bin not found" >&2; exit 1; }
dir=$(dirname "$bin")

die() { echo "error: $*" >&2; exit 1; }

elf_build_id() { readelf -n "$1" | awk '/Build ID:/ {print $3}'; }
macho_uuid() { dwarfdump --uuid "$1" | awk 'NR == 1 {print $2}'; }

case "$(uname -s)" in
  Linux)
    dbg="$dir/$name.debug"
    objcopy --only-keep-debug "$bin" "$dbg"
    strip --strip-all "$bin"
    objcopy --add-gnu-debuglink="$dbg" "$bin"

    if readelf -SW "$bin" | grep -qE '\.debug_|\.symtab'; then
      die "$bin still has debug sections or a symbol table"
    fi
    readelf -SW "$dbg" | grep -q '\.debug_line' || die "$dbg has no line tables"
    id_bin=$(elf_build_id "$bin")
    id_dbg=$(elf_build_id "$dbg")
    ;;
  Darwin)
    dbg="$dir/$name.dSYM"
    rm -rf "$dbg"
    dsymutil "$bin" -o "$dbg"
    strip "$bin"
    # strip invalidates the linker's ad-hoc signature; arm64 macOS will
    # not run an unsigned binary.
    codesign --force --sign - "$bin"

    dwarf="$dbg/Contents/Resources/DWARF/$(basename "$bin")"
    [[ -f "$dwarf" ]] || die "$dwarf missing"
    # dsymutil writes an empty dSYM when it cannot find the objects
    # (for example a deleted LTO object), so count the compile units.
    units=$(dwarfdump --debug-info "$dwarf" | grep -c DW_TAG_compile_unit || true)
    (( units > 0 )) || die "$dbg has no compile units (debug info not found)"
    id_bin=$(macho_uuid "$bin")
    id_dbg=$(macho_uuid "$dwarf")
    ;;
  *) die "unsupported OS $(uname -s)" ;;
esac

[[ -n "$id_bin" ]] || die "$bin has no build ID / UUID"
[[ "$id_bin" == "$id_dbg" ]] || die "build ID mismatch: $bin $id_bin, $dbg $id_dbg"
echo "ok: $(basename "$bin") stripped, debug info in $(basename "$dbg") (id $id_bin)"
