#!/bin/sh
#
# make-tbd.sh -- write a text-based stub for a system library.
#
# The linker needs to know which symbols a library exports; it does not
# need the library.  That is what a .tbd is, and it is what an SDK ships
# instead of the dylibs themselves.
#
# On a current macOS the libraries are not on disk to be read: they live
# in the dyld shared cache, and the files under /usr/lib are truncated
# placeholders that llvm-readtapi -stubify rejects, correctly.  The
# exports are still readable from the cache, so the stub is built from
# those rather than from a file.
#
# libSystem is an umbrella: it exports almost nothing itself and
# re-exports some thirty libraries under /usr/lib/system.  Their symbols
# are gathered into the one stub, which is what a program linking
# -lSystem expects to find.
#
# Usage: make-tbd.sh <install-name> <output.tbd>
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

[ $# -eq 2 ] || { echo "usage: $0 <install-name> <output.tbd>" >&2; exit 1; }

INSTALL_NAME="$1"
OUTPUT="$2"
TMP="${OUTPUT}.symbols.$$"

command -v dyld_info >/dev/null 2>&1 || {
	echo "$0: dyld_info is required to read the shared cache" >&2
	exit 1
}

# Every library whose symbols belong in this stub: the library itself,
# plus anything it re-exports.
libs="$INSTALL_NAME"
reexports=$(dyld_info -dependents "$INSTALL_NAME" 2>/dev/null |
	awk '/re-export/ { print $NF }')
[ -n "$reexports" ] && libs="$libs $reexports"

: > "$TMP"
for lib in $libs; do
	# The listing is "offset symbol"; take the symbol, and skip the
	# header lines and anything that is not one.
	dyld_info -exports "$lib" 2>/dev/null |
		awk 'NF == 2 && $1 ~ /^0x/ { print $2 }' >> "$TMP" || true
done

sort -u "$TMP" -o "$TMP"
count=$(wc -l < "$TMP" | tr -d ' ')

if [ "$count" -eq 0 ]; then
	rm -f "$TMP"
	echo "$0: no exports found for $INSTALL_NAME" >&2
	exit 1
fi

{
	echo "--- !tapi-tbd"
	echo "tbd-version:           4"
	echo "targets:               [ arm64-macos, arm64e-macos, x86_64-macos ]"
	echo "install-name:          $INSTALL_NAME"
	echo "current-version:       1"
	echo "compatibility-version: 1"
	echo "exports:"
	echo "  - targets:              [ arm64-macos, arm64e-macos, x86_64-macos ]"
	printf "    symbols:              [ "
	# One symbol per line, wrapped as the format expects.
	awk 'NR > 1 { printf ",\n                            " } { printf "%s", $0 }' "$TMP"
	echo " ]"
	echo "..."
} > "$OUTPUT"

rm -f "$TMP"
echo "  $INSTALL_NAME: $count symbols"
