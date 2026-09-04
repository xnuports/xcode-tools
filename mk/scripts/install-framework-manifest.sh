#!/bin/sh
#
# install-framework-manifest.sh MANIFEST DEST MODULEMAP UMBRELLA LANG SRCROOT...
#
# Install a framework's public headers from a manifest, and write a
# module map over the ones that compile.
#
# The manifest is the list of paths Apple ship, relative to the
# framework's Headers directory.  Each is looked for by name across the
# source roots given, in order, and installed where the manifest says
# -- the source trees lay their headers out by role rather than by the
# shape the framework has, and copying them wholesale would carry in
# everything else they contain.
#
# A header is declared in the module map only if it compiles on its
# own.  What fails is still installed, since Apple ship it; it is left
# undeclared, which is what this tree's other module maps do with the
# headers they cannot take.  CC and SDK come from the environment.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

MANIFEST=$1; shift
DEST=$1; shift
MM=$1; shift
UMBRELLA=$1; shift
LANG=$1; shift

[ -n "$MANIFEST" ] && [ -n "$DEST" ] || {
	echo "usage: $0 MANIFEST DEST MODULEMAP UMBRELLA LANG SRCROOT..." >&2
	exit 1; }
[ -f "$MANIFEST" ] || { echo "$0: no manifest at $MANIFEST" >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$DEST"

INDEX="$DEST/.index"
: > "$INDEX"
for root in "$@"; do
	[ -d "$root" ] || continue
	find "$root" -name '*.h' -not -path '*/.git/*' 2>/dev/null >> "$INDEX"
done

missing=0
while read -r h; do
	case "$h" in ''|\#*) continue;; esac
	base=${h##*/}
	src=$(grep -m1 "/$base\$" "$INDEX" 2>/dev/null || true)
	if [ -z "$src" ]; then
		missing=$((missing + 1))
		continue
	fi
	mkdir -p "$DEST/$(dirname "$h")"
	cp -f "$src" "$DEST/$h"
done < "$MANIFEST"
rm -f "$INDEX"

name=$(basename "$UMBRELLA" .h)

if [ -n "$MM" ]; then
	BAD="$DEST/.undeclarable"
	: > "$BAD"
	( cd "$DEST" && find . -name '*.h' | sed 's|^\./||' | sort ) > "$DEST/.all"

	if [ -n "$CC" ] && [ -n "$SDK" ]; then
		probe="$DEST/.probe.m"
		while read -r h; do
			printf '#import <%s/%s>\nint _xt_probe;\n' "$name" "$h" > "$probe"
			# $CC unquoted: the make variable is more than one word.
			if ! $CC -isysroot "$SDK" -x "$LANG" -fsyntax-only \
			     "$probe" >/dev/null 2>&1; then
				echo "$h" >> "$BAD"
			fi
		done < "$DEST/.all"
		rm -f "$probe"
	fi

	mkdir -p "$(dirname "$MM")"
	{
		echo "framework module $name [system] {"
		while read -r h; do
			grep -qxF "$h" "$BAD" && continue
			echo "    header \"$h\""
		done < "$DEST/.all"
		echo ""
		echo "    export *"
		echo "}"
	} > "$MM"
	dec=$(grep -c 'header "' "$MM" || true)
	rm -f "$BAD" "$DEST/.all"
else
	dec=0
fi

all=$(find "$DEST" -name '*.h' | wc -l | tr -d ' ')
if [ "$missing" -gt 0 ]; then
	echo "    $name: $all headers, $dec declared ($missing of the manifest not found)"
else
	echo "    $name: $all headers, $dec declared"
fi
