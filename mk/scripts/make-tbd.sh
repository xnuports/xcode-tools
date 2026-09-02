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

[ $# -ge 2 ] && [ $# -le 3 ] ||
	{ echo "usage: $0 <install-name> <output.tbd> [sdk-include-dir]" >&2
	  exit 1; }

INSTALL_NAME="$1"
OUTPUT="$2"
TMP="${OUTPUT}.symbols.$$"
TMP_X86="${OUTPUT}.x86.$$"
SDK_INC="$3"

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
	# Two shapes appear in the listing.  Most symbols are "offset
	# symbol"; a symbol the library re-exports from another is
	# "[re-export] _name (_other from libfoo)" instead.  Taking only
	# the first shape loses the second, and the second is where the
	# string and memory routines live -- libsystem_c re-exports
	# _strlen from libsystem_platform, so a stub built without them
	# compiles C and then fails to link anything using <string>.
	dyld_info -exports "$lib" 2>/dev/null | awk '
		NF == 2 && $1 ~ /^0x/  { print $2 }
		$1 == "[re-export]"    { print $2 }
	' >> "$TMP" || true
done

sort -u "$TMP" -o "$TMP"

# The symbols x86_64 has and arm64 does not.
#
# A few libc functions are declared with an assembler name that carries
# a suffix on some architectures and not others: sys/cdefs.h gives
# __DARWIN_SUF_64_BIT_INO_T and __DARWIN_SUF_1050 a value only where the
# old 32-bit-inode and pre-10.5 entry points still exist, which on macOS
# means x86_64 and not arm64.  So on x86_64 the compiler emits
# _stat$INODE64 where on arm64 it emits _stat.
#
# The exports read above come from this machine's shared cache, which is
# arm64: it has no idea those variants exist, and a stub built from it
# alone links every arm64 program and fails the first x86_64 one that
# calls stat or opendir.  There is no x86_64 cache this can read, but
# there is something better -- the headers declare exactly which
# functions take which suffix, and they are the same declarations the
# compiler reads, so the two cannot disagree.
: > "$TMP_X86"

if [ -n "$SDK_INC" ] && [ -d "$SDK_INC" ]; then
	for spec in "INODE64:__DARWIN_INODE64 __DARWIN_ALIAS_I" \
	            "1050:__DARWIN_1050 __DARWIN_1050ALIAS"; do
		suffix=${spec%%:*}
		macros=${spec#*:}

		for m in ${macros}; do
			grep -rhoE "${m}\([a-z_0-9]+\)" "$SDK_INC" 2>/dev/null |
			    sed -e "s/^${m}(//" -e 's/)$//'
		done | sort -u | while read -r name; do
			[ -n "$name" ] || continue

			# Only where this library exports the plain symbol:
			# the same scan runs for libc++ and libobjc, which
			# provide none of these.  Written as an if, because
			# a bare grep that finds nothing is a failing
			# command and set -e would end the script on the
			# first symbol this library does not have.
			if grep -qx "_${name}" "$TMP"; then
				echo "_${name}\$${suffix}"
			fi
		done
	done > "$TMP_X86"

	sort -u "$TMP_X86" -o "$TMP_X86"
fi

x86count=$(wc -l < "$TMP_X86" | tr -d ' ')
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

	if [ "$x86count" -gt 0 ]; then
		echo "  - targets:              [ x86_64-macos ]"
		printf "    symbols:              [ "
		awk 'NR > 1 { printf ",\n                            " } { printf "%s", $0 }' "$TMP_X86"
		echo " ]"
	fi

	echo "..."
} > "$OUTPUT"

rm -f "$TMP" "$TMP_X86"

if [ "$x86count" -gt 0 ]; then
	echo "  $INSTALL_NAME: $count symbols (+$x86count for x86_64)"
else
	echo "  $INSTALL_NAME: $count symbols"
fi
