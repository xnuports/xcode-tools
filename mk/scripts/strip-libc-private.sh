#!/bin/sh
#
# strip-libc-private.sh -- remove Libc's internal sections from an
# installed header.
#
# Libc's headers carry blocks meant only for building Libc itself,
# delimited by //Begin-Libc and //End-Libc.  _ctype.h opens with one that
# includes xlocale_private.h and mblocal.h -- headers that exist in the
# source and are not part of an SDK, which is why Apple's copy of
# _ctype.h has no such include and their SDK has no such file.  Their
# header install strips these; so does this.
#
# Without it the SDK compiles C but not C++: <string> reaches _ctype.h
# and stops on a header no SDK has ever shipped.
#
# Usage: strip-libc-private.sh <dir>   (edits *.h in place, recursively)
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e
[ $# -eq 1 ] || { echo "usage: $0 <dir>" >&2; exit 1; }

find "$1" -name '*.h' -type f | while read -r h; do
	grep -q '//Begin-Libc' "$h" 2>/dev/null || continue
	awk '
		/\/\/Begin-Libc/ { skip = 1; next }
		/\/\/End-Libc/   { skip = 0; next }
		!skip            { print }
	' "$h" > "$h.stripped" && mv "$h.stripped" "$h"
done
