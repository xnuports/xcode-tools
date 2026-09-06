#!/bin/sh
#
# install-sdk-headers.sh -- fill in a platform SDK's usr/include.
#
# The C headers of every Darwin platform are the macOS ones, restricted to
# what that platform is allowed to reach for -- DriverKit drops everything a
# userspace driver has no business calling, iPhoneOS and its relatives drop a
# smaller set.  They come from the same open sources, so rather than
# assembling them once per platform this installs them out of the MacOSX SDK
# that has already been built here, following a per-platform manifest under
# lib/.
#
# The copies are not byte-for-byte Apple's.  Apple's DriverKit stdio.h is
# three lines from Apple's macOS stdio.h, which is also how far ours is from
# Apple's DriverKit one; in one case it is much further, os/log.h being a
# genuinely different header there.  Those differences are Apple's own
# SDK-vending edits, not a different source, and closing them is separate
# work.
#
# Anything the manifest names that this build has not produced is counted and
# reported rather than passed over, so the shortfall stays in view.
#
# Usage: install-sdk-headers.sh <label> <manifest> <macos-include-dir> <dest>
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

[ $# -eq 4 ] ||
	{ echo "usage: $0 <label> <manifest> <macos-include-dir> <dest>" >&2; exit 1; }

LABEL="$1"
MANIFEST="$2"
SRC="$3"
DEST="$4"

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

# Apple's platform headers are not always byte-for-byte the macOS ones, and
# where they differ they sometimes drop an include: DriverKit's sys/_types.h does
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
	[ "${added}" -gt 0 ] || break
done

# Counted from what is on disk rather than from what this run copied: the
# manifest is installed unconditionally every time, but the closure only adds
# what is missing, so counting additions made a rebuild report "+ 0" for a
# tree that still holds all of them.
present=$(find "${DEST}" -type f ! -name '.*' | wc -l | tr -d ' ')
extra=$((present - installed))

if [ "${missing}" -gt 0 ]; then
	# One line per platform: name the first few and count the rest, rather
	# than printing three hundred of them across the build log.
	all=$(sed 's|/.*||' "${MISSING_LIST}" | sort -u)
	count=$(echo "${all}" | wc -l | tr -d ' ')
	where=$(echo "${all}" | head -8 | tr '\n' ' ')
	[ "${count}" -gt 8 ] && where="${where}and $((count - 8)) more"
	echo "    ${LABEL}: ${installed} headers + ${extra} to close their" \
	     "includes, ${missing} not built here (${where})"
else
	echo "    ${LABEL}: ${installed} headers + ${extra} to close their includes"
fi

rm -f "${MISSING_LIST}"
