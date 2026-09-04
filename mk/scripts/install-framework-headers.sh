#!/bin/sh
#
# install-framework-headers.sh SRC DEST FRAMEWORK
#
# Install a framework's public headers into an SDK framework directory.
#
# Apple's open source drops carry both the public headers and the ones
# that only their own .c files include.  The public set is not listed
# anywhere in them, but it does not need to be: it is whatever the
# umbrella header reaches.  So the umbrella is walked and its
# transitive <FRAMEWORK/...> includes are installed, which keeps the
# module self-consistent -- every header the umbrella names is present
# -- without a hand-maintained list to drift out of date.
#
# FRAMEWORK defaults to CoreFoundation and names both the umbrella
# header (FRAMEWORK.h) and the include prefix to follow.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

SRC=$1
DEST=$2
FW=${3:-CoreFoundation}

[ -n "$SRC" ] && [ -n "$DEST" ] || { echo "usage: $0 SRC DEST [FRAMEWORK]" >&2; exit 1; }
[ -f "$SRC/$FW.h" ] || { echo "$0: no $FW.h in $SRC" >&2; exit 1; }

mkdir -p "$DEST"

# Breadth-first over the umbrella's includes.  A header already in
# $DEST has been done, which doubles as the visited set.
todo="$FW.h"
while [ -n "$todo" ]; do
	next=""
	for h in $todo; do
		[ -f "$SRC/$h" ] || continue
		[ -f "$DEST/$h" ] && continue
		cp -f "$SRC/$h" "$DEST/$h"
		deps=$(sed -n "s|.*<$FW/\\([A-Za-z0-9_]*\\.h\\)>.*|\\1|p" \
		    "$SRC/$h" 2>/dev/null || true)
		for d in $deps; do
			[ -f "$DEST/$d" ] || next="$next $d"
		done
	done
	todo=$next
done

# One header the umbrella does not reach.  CoreFoundation.h includes
# CFMessagePort.h and not CFMachPort.h, though the drop carries both and
# Apple ship both; the omission is upstream's.  CFMachPortCreate() and
# its neighbours are public API, and IOKit's private headers include
# <CoreFoundation/CFMachPort.h> directly.
if [ "$FW" = CoreFoundation ] && [ -f "$SRC/CFMachPort.h" ] && [ ! -f "$DEST/CFMachPort.h" ]; then
	cp -f "$SRC/CFMachPort.h" "$DEST/CFMachPort.h"
fi

echo "    $FW: $(ls "$DEST"/*.h 2>/dev/null | wc -l | tr -d ' ') headers"
