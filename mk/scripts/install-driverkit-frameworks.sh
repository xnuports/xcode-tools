#!/bin/sh
#
# install-driverkit-frameworks.sh MANIFEST SRCROOT DEST
#
# Assemble the DriverKit SDK's frameworks.
#
# A DriverKit framework header is a C++ class declaration, usually paired
# with the .iig the interface generator reads, and it carries Apple's
# OSReference licence header.  They are source, not metadata recovered from a
# binary -- no class-dump produces them, and the frameworks on a Mac that has
# no driver extension installed hold nothing but an Info.plist anyway.
#
# So they are gathered the way IOKit.framework's are: each name in the
# manifest is looked for across the open-source drops this tree has --
# xnu's iokit/DriverKit, IOHIDFamily's HIDDriverKit, IOStorageFamily's
# BlockStorageDeviceDriverKit and the rest -- and installed at the path the
# manifest gives, whatever layout the project it came from uses.
#
# Where a family's DriverKit headers have not been released there is nothing
# to find.  USBDriverKit, NetworkingDriverKit, AudioDriverKit, MIDIDriverKit,
# VideoDriverKit, PCIDriverKit, the SCSI ones and the serial ones are all in
# that position today: Apple ship the frameworks but not their sources.
# Those are counted and named rather than passed over.
#
# Usage: install-driverkit-frameworks.sh <manifest> <src-root> <dest>
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

[ $# -eq 3 ] ||
	{ echo "usage: $0 <manifest> <src-root> <dest>" >&2; exit 1; }

MANIFEST="$1"
SRCROOT="$2"
DEST="$3"

[ -f "${MANIFEST}" ] || { echo "$0: no manifest at ${MANIFEST}" >&2; exit 1; }
[ -d "${SRCROOT}" ] || { echo "$0: no source root at ${SRCROOT}" >&2; exit 1; }

INDEX="${DEST}/.index.$$"
mkdir -p "${DEST}"

# One pass over the sources, indexed by basename.  Searching by name rather
# than copying trees is deliberate: the families carry their kernel-side
# headers beside the driver-side ones and Apple ship none of those.
find "${SRCROOT}" -type f \( -name '*.h' -o -name '*.iig' \) \
     -not -path '*/.git/*' 2>/dev/null |
	awk -F/ '{ print $NF "\t" $0 }' | LC_ALL=C sort -u > "${INDEX}"

installed=0
missing=0
MISSING_LIST="${DEST}/.missing.$$"
: > "${MISSING_LIST}"

while read -r entry; do
	case "${entry}" in
	''|'#'*) continue ;;
	esac

	framework=${entry%%/*}
	relative=${entry#*/}
	base=${relative##*/}

	source=$(LC_ALL=C awk -F'\t' -v n="${base}" \
	         '$1 == n { print $2; exit }' "${INDEX}")

	if [ -n "${source}" ]; then
		target="${DEST}/${framework}.framework/Headers/${relative}"
		mkdir -p "$(dirname "${target}")"
		cp -f "${source}" "${target}"
		installed=$((installed + 1))
	else
		echo "${framework}" >> "${MISSING_LIST}"
		missing=$((missing + 1))
	fi
done < "${MANIFEST}"

if [ "${missing}" -gt 0 ]; then
	where=$(sort -u "${MISSING_LIST}" | tr '\n' ' ')
	echo "    DriverKit frameworks: ${installed} headers," \
	     "${missing} not released by Apple (${where})"
else
	echo "    DriverKit frameworks: ${installed} headers"
fi

rm -f "${INDEX}" "${MISSING_LIST}"
