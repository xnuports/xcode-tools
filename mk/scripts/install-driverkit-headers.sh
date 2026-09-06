#!/bin/sh
#
# install-driverkit-headers.sh -- fill in a DriverKit SDK's usr/include.
#
# DriverKit's C headers are the macOS ones, restricted to what a userspace
# driver may reach for.  They come from the same open sources, so rather than
# assembling them a second time this installs them out of the MacOSX SDK that
# has already been built here, following the manifest in
# lib/driverkit-headers.txt.
#
# The two differ a little in content -- Apple's DriverKit stdio.h is three
# lines from Apple's macOS stdio.h, which is also how far ours is from
# Apple's DriverKit one -- and in one case a lot: os/log.h is a genuinely
# different header there.  Those differences are Apple's own SDK-vending
# edits, not a different source, and closing them is separate work.
#
# Anything the manifest names that this build has not produced is counted and
# reported rather than passed over, so the shortfall stays in view.
#
# Usage: install-driverkit-headers.sh <manifest> <macos-include-dir> <dest>
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

[ $# -eq 3 ] ||
	{ echo "usage: $0 <manifest> <macos-include-dir> <dest>" >&2; exit 1; }

MANIFEST="$1"
SRC="$2"
DEST="$3"

[ -f "${MANIFEST}" ] || { echo "$0: no manifest at ${MANIFEST}" >&2; exit 1; }
[ -d "${SRC}" ] || { echo "$0: no macOS include tree at ${SRC}" >&2; exit 1; }

installed=0
missing=0
MISSING_LIST="${DEST}/.missing.$$"

mkdir -p "${DEST}"
: > "${MISSING_LIST}"

while read -r header; do
	case "${header}" in
	''|'#'*) continue ;;
	esac

	if [ -f "${SRC}/${header}" ]; then
		mkdir -p "${DEST}/$(dirname "${header}")"
		cp -f "${SRC}/${header}" "${DEST}/${header}"
		installed=$((installed + 1))
	else
		echo "${header}" >> "${MISSING_LIST}"
		missing=$((missing + 1))
	fi
done < "${MANIFEST}"

# Apple's DriverKit headers are not always byte-for-byte the macOS ones, and
# where they differ they sometimes drop an include: their sys/_types.h does
# not reach for the pthread types, because a driver has no pthreads, so their
# manifest has no sys/_pthread/ in it.  Ours does reach for them, and stopping
# at the manifest would leave the include dangling and the SDK unusable.
#
# So the graph is closed: anything an installed header includes, that this
# build has and has not installed yet, is installed too.  That is a deliberate
# departure from Apple's file list -- an SDK that compiles is worth more than
# one that matches a listing -- and the extras are counted so it stays a
# visible departure rather than a quiet one.
extra=0
while :; do
	# Both spellings.  A quoted include is looked up beside the file that
	# wrote it before it is looked up at the root -- machine/signal.h asks
	# for "arm/signal.h" and means the one at the root -- so both
	# candidates are offered and whichever exists is taken.
	find "${DEST}" -name '*.h' -exec awk -v root="${DEST}/" '
	    /^[[:space:]]*#[[:space:]]*(include|import)[[:space:]]*[<"]/ {
		if (match($0, /<[^>]*>/)) {
			print substr($0, RSTART + 1, RLENGTH - 2)
			next
		}
		if (match($0, /"[^"]*"/)) {
			want = substr($0, RSTART + 1, RLENGTH - 2)
			print want
			dir = FILENAME
			sub(/\/[^\/]*$/, "", dir)
			if (substr(dir, 1, length(root)) == root)
				print substr(dir, length(root) + 1) "/" want
		}
	    }' {} + | sort -u > "${DEST}/.wanted.$$"

	added=0
	while read -r want; do
		[ -n "${want}" ] || continue
		[ -f "${DEST}/${want}" ] && continue
		[ -f "${SRC}/${want}" ] || continue
		mkdir -p "${DEST}/$(dirname "${want}")"
		cp -f "${SRC}/${want}" "${DEST}/${want}"
		added=$((added + 1))
	done < "${DEST}/.wanted.$$"

	rm -f "${DEST}/.wanted.$$"
	extra=$((extra + added))
	[ "${added}" -gt 0 ] || break
done

if [ "${missing}" -gt 0 ]; then
	where=$(sed 's|/.*||' "${MISSING_LIST}" | sort -u | tr '\n' ' ')
	echo "    DriverKit: ${installed} headers + ${extra} to close their" \
	     "includes, ${missing} not built here (${where})"
else
	echo "    DriverKit: ${installed} headers + ${extra} to close their includes"
fi

rm -f "${MISSING_LIST}"
