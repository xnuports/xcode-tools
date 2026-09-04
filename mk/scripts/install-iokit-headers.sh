#!/bin/sh
#
# install-iokit-headers.sh MANIFEST IOKITUSER FAKEROOT SRCROOT DEST
#                          [MODULEMAP [PRIVATE_DEST]]
#
# CC and SDK come from the environment, as they do for
# emit-darwin-modulemap.sh: the make variable for the compiler carries
# more than one word, and passing it positionally shifts everything
# after it.
#
# Assemble IOKit.framework's headers.
#
# The set is the manifest, lib/iokit-headers.txt -- the paths Apple
# ship, recorded because they cannot be derived here.  Each is looked
# for by name across the sources this tree has, in order:
#
#   the fakeroot's IOKit.framework   xnu's own userland install
#   IOKitUser                        the framework's own project
#   the driver families              IOStorageFamily, IOHIDFamily, ...
#
# and installed at the path the manifest gives, whatever layout the
# project it came from happens to use: IOMedia.h sits at the top of
# IOStorageFamily and ships as storage/IOMedia.h, IOSerialKeys.h is
# under a .kmodproj and ships as serial/IOSerialKeys.h.
#
# Searching by name rather than copying trees is deliberate.  The
# families carry their kernel-side headers too -- IOStorageFamily has
# IOBlockStorageDriver's internals beside the ones in the SDK -- and
# Apple ship none of those.
#
# IOKitUser also carries its own internal headers, and those are not
# part of the framework Apple ship: IOKitLibPrivate.h, the *Internal.h
# and *Priv*.h files, the mig-generated ones, and the whole of
# kext.subproj beyond KextManager.h.  Installed into the public
# framework they break it -- IOKitLibPrivate.h includes
# <CoreFoundation/CFMachPort.h> and appears in the module map, so
# building the module fails.  PRIVATE_DEST, when given, takes them
# instead, which is how the internal SDK gets them.
#
# This is not all of Apple's IOKit.  Theirs carries 144 headers and
# these two sources supply 44; the rest belong to the driver families
# -- IOStorageFamily, IONetworkingFamily, IOHIDFamily, IOGraphics,
# IOUSBFamily, IOFireWireFamily and the SCSI ones -- each its own
# project.  What is here is the core: IOKitLib.h and everything it
# needs, which is what a tool that talks to the registry uses.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

MANIFEST=$1
IKU=$2
FAKEROOT=$3
SRCROOT=$4
DEST=$5
MM=$6
PRIVATE_DEST=$7

# What Apple do not ship in the public framework.  Matched on the name
# where the name says so, and listed where it does not.
is_private() {
	case "$1" in
	*Private.h|*Priv.h|*Internal.h|*_mig.h|*mig.h)	return 0 ;;
	IOSystemConfiguration.h|IOMIGMachPort.h)	return 0 ;;
	esac
	# kext.subproj is an implementation of kextd and kextcache; only
	# KextManager.h is API.
	case "$2/$1" in
	kext/KextManager.h)	return 1 ;;
	kext/*)			return 0 ;;
	esac
	return 1
}

[ -n "$IKU" ] && [ -n "$FAKEROOT" ] && [ -n "$DEST" ] || {
	echo "usage: $0 IOKITUSER FAKEROOT DEST [MODULEMAP]" >&2; exit 1; }
[ -d "$IKU" ] || { echo "$0: no IOKitUser at $IKU" >&2; exit 1; }

mkdir -p "$DEST"

# Build one index of every candidate header, by basename, in priority
# order -- the first hit wins, so xnu's installed copy beats a driver
# family's source copy of the same name.
INDEX="$DEST/.index"
mkdir -p "$DEST"
: > "$INDEX"

for root in \
    "$FAKEROOT/System/Library/Frameworks/IOKit.framework/Versions/A/Headers" \
    "$IKU" \
    "$SRCROOT/xnu/iokit/IOKit" \
    "$SRCROOT"/IOStorageFamily "$SRCROOT"/IONetworkingFamily \
    "$SRCROOT"/IOHIDFamily "$SRCROOT"/IOGraphics \
    "$SRCROOT"/IOFireWireFamily "$SRCROOT"/IOSCSIArchitectureModelFamily \
    "$SRCROOT"/IOUSBFamily "$SRCROOT"/IOAudioFamily "$SRCROOT"/IOSerialFamily
do
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
		echo "$h" >> "$DEST/.notfound"
		continue
	fi
	mkdir -p "$DEST/$(dirname "$h")"
	cp -f "$src" "$DEST/$h"
done < "$MANIFEST"

# IOKitUser's own internal headers, which Apple ship nowhere, go to the
# private destination if one was given.
if [ -n "$PRIVATE_DEST" ]; then
	for h in "$IKU"/*.h; do
		[ -f "$h" ] || continue
		b=$(basename "$h")
		is_private "$b" "" || continue
		mkdir -p "$PRIVATE_DEST"
		cp -f "$h" "$PRIVATE_DEST/"
	done
	for d in "$IKU"/*.subproj; do
		[ -d "$d" ] || continue
		name=$(basename "$d" .subproj)
		for h in "$d"/*.h; do
			[ -f "$h" ] || continue
			b=$(basename "$h")
			is_private "$b" "$name" || continue
			mkdir -p "$PRIVATE_DEST/$name"
			cp -f "$h" "$PRIVATE_DEST/$name/"
		done
	done
fi

rm -f "$INDEX"

# A header is only declared if it actually compiles.
#
# Two things rule one out.  The families are incomplete, so a header
# reaching into one that is absent cannot be named -- audio/IOAudioLib.h
# includes <IOKit/audio/IOAudioTypes.h> and naming it would make the
# whole module unbuildable.  And several of these compile only inside
# the kernel: graphics/IOGraphicsEngine.h wants IOOptionBits and
# IOByteCount, which are IOKit's kernel types and are in no userland
# header.
#
# Reachability alone catches the first and not the second, so each
# candidate is compiled.  What fails is still installed -- it is a
# header Apple ship and someone may include it deliberately -- it is
# just left out of the module map, which is what the Darwin map does
# with the headers it cannot take.
BAD="$DEST/.undeclarable"
: > "$BAD"

( cd "$DEST" && find . -name '*.h' | sed 's|^\./||' | sort ) > "$DEST/.allheaders"

if [ -n "$CC" ] && [ -n "$SDK" ]; then
	probe="$DEST/.probe.c"
	while read -r h; do
		printf '#include <IOKit/%s>\nint _xt_probe;\n' "$h" > "$probe"
		# $CC unquoted on purpose: the make variable for the
		# compiler is more than one word, and quoting it makes
		# the whole thing a command name that does not exist --
		# every probe then "fails" and nothing is declared.
		if ! $CC -isysroot "$SDK" -fsyntax-only "$probe" >/dev/null 2>&1; then
			echo "$h" >> "$BAD"
		fi
	done < "$DEST/.allheaders"
	rm -f "$probe"
fi

declarable() {
	! grep -qxF "$1" "$BAD"
}

# The module map, written from what was actually installed.
#
# Apple's lists every header by name rather than taking an umbrella,
# and marks the module extern_c -- these are C headers and anything
# importing them from C++ needs that.  Theirs cannot be copied: it
# names the hundred family headers this tree does not have, and a
# module map that names a missing header does not build.  So it is
# generated: the top level by name, then one explicit submodule per
# directory, which is the same shape.
if [ -n "$MM" ]; then
	mkdir -p "$(dirname "$MM")"
	{
		echo "framework module IOKit [extern_c] [system] {"
		( cd "$DEST" && ls *.h 2>/dev/null ) | while read -r h; do
			declarable "$h" || continue
			echo "    header \"$h\""
		done
		echo ""
		echo "    export *"
		for d in "$DEST"/*/; do
			[ -d "$d" ] || continue
			n=$(basename "$d")
			ls "$d"/*.h >/dev/null 2>&1 || continue
			echo ""
			echo "    explicit module $n {"
			( cd "$d" && ls *.h ) | while read -r h; do
				declarable "$n/$h" || continue
				echo "        header \"$n/$h\""
			done
			echo "        export *"
			echo "    }"
		done
		echo "}"
	} > "$MM"
fi

rm -f "$BAD" "$DEST/.allheaders"

n=$(find "$DEST" -name '*.h' | wc -l | tr -d ' ')
if [ "$missing" -gt 0 ]; then
	echo "    IOKit: $n headers ($missing of the manifest not in this tree)"
	rm -f "$DEST/.notfound"
else
	echo "    IOKit: $n headers"
fi
