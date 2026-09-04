#!/bin/sh
#
# install-iokit-headers.sh IOKITUSER FAKEROOT DEST [MODULEMAP [PRIVATE_DEST]]
#
# Assemble IOKit.framework's headers.
#
# They come from two places.  xnu installs a userland IOKit.framework
# of its own -- the types, return codes and keys that the kernel and
# userland agree on -- and that is taken as installed, from the
# fakeroot.  The rest is IOKitUser, whose layout maps onto the
# framework by a single rule: each <name>.subproj becomes IOKit/<name>,
# and what sits at the top becomes IOKit itself.  IOHIDLib.h is in
# hid.subproj and Apple ship it as hid/IOHIDLib.h; IOPMLib.h is in
# pwr_mgt.subproj and ships as pwr_mgt/IOPMLib.h, and so on.
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

IKU=$1
FAKEROOT=$2
DEST=$3
MM=$4
PRIVATE_DEST=$5

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

# xnu's installed framework first.  Its own Versions/A/Headers is the
# header root -- copying from the bundle root would carry the Versions
# directory into the destination as if it were a header directory.
FW=$FAKEROOT/System/Library/Frameworks/IOKit.framework/Versions/A/Headers
if [ -d "$FW" ]; then
	( cd "$FW" && find . -name '*.h' | while read -r h; do
		mkdir -p "$DEST/$(dirname "$h")"
		cp -f "$h" "$DEST/$h"
	done )
fi

# IOKitUser's top level.
for h in "$IKU"/*.h; do
	[ -f "$h" ] || continue
	b=$(basename "$h")
	if is_private "$b" ""; then
		[ -n "$PRIVATE_DEST" ] || continue
		mkdir -p "$PRIVATE_DEST"
		cp -f "$h" "$PRIVATE_DEST/"
	else
		cp -f "$h" "$DEST/"
	fi
done

# Then each subproject, under the name it ships as.
for d in "$IKU"/*.subproj; do
	[ -d "$d" ] || continue
	name=$(basename "$d" .subproj)
	mkdir -p "$DEST/$name"
	for h in "$d"/*.h; do
		[ -f "$h" ] || continue
		b=$(basename "$h")
		if is_private "$b" "$name"; then
			[ -n "$PRIVATE_DEST" ] || continue
			mkdir -p "$PRIVATE_DEST/$name"
			cp -f "$h" "$PRIVATE_DEST/$name/"
		else
			cp -f "$h" "$DEST/$name/"
		fi
	done
done

# A header is only declared if everything it reaches is here.
#
# The families are missing, and a header that reaches into one cannot
# be part of the module: audio/IOAudioLib.h includes
# <IOKit/audio/IOAudioTypes.h>, which belongs to IOAudioFamily, so
# naming it makes the whole module unbuildable.  Such headers are still
# installed -- they compile for anyone who has the family -- they are
# simply left undeclared, which is what the Darwin module map does with
# the headers it cannot take.
#
# The reachability has to be transitive.  Checking one level only
# catches audio/IOAudioLib.h and still declares hid/IOHIDManager.h,
# which includes hid/IOHIDBase.h, which includes the hid/IOHIDKeys.h
# that IOHIDFamily owns.  So the set is closed by iterating to a fixed
# point: anything including something already ruled out is ruled out
# too, until a pass changes nothing.
BAD="$DEST/.undeclarable"
: > "$BAD"

( cd "$DEST" && find . -name '*.h' | sed 's|^\./||' | sort ) > "$DEST/.allheaders"

while :; do
	changed=0
	while read -r h; do
		grep -qxF "$h" "$BAD" && continue
		deps=$(sed -n 's|.*<IOKit/\([A-Za-z0-9_./]*\.h\)>.*|\1|p' "$DEST/$h" 2>/dev/null || true)
		for d in $deps; do
			if [ ! -f "$DEST/$d" ] || grep -qxF "$d" "$BAD"; then
				echo "$h" >> "$BAD"
				changed=1
				break
			fi
		done
	done < "$DEST/.allheaders"
	[ "$changed" -eq 0 ] && break
done

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
MM=$4
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

echo "    IOKit: $(find "$DEST" -name '*.h' | wc -l | tr -d ' ') headers"
