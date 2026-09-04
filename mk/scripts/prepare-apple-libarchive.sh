#!/bin/sh
#
# prepare-apple-libarchive.sh [DIR]
#
# Teach upstream's CMake build about Apple's additions.
#
# Apple's drop is their Xcode project wrapped around upstream's tree,
# and the files they add are listed only in the xcodeproj.  Building
# through upstream's CMakeLists therefore compiles none of them --
# archive_check_entitlement.c above all, which every
# archive_read_support_filter_* calls through
# archive_allow_entitlement_filter(), and archive_mac.c, which carries
# the quarantine plumbing archive_read_open_filename.c calls.  The
# library builds and then nothing links.
#
# So the file is added to libarchive_SOURCES, and the two frameworks it
# uses are linked: it asks SecTaskCopyValueForEntitlement() whether the
# calling process is allowed a given filter, which is CoreFoundation
# and Security.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

DIR=${1:-.}
CM=$DIR/libarchive/libarchive/CMakeLists.txt

[ -f "$CM" ] || { echo "$0: no libarchive/libarchive/CMakeLists.txt under $DIR" >&2; exit 1; }

grep -q 'archive_check_entitlement.c' "$CM" && exit 0

awk '
	/^SET\(libarchive_SOURCES$/ {
		print
		print "  archive_check_entitlement.c"
		print "  archive_check_entitlement.h"
		print "  archive_mac.c"
		print "  archive_mac.h"
		added = 1
		next
	}
	{ print }
	END { if (!added) exit 1 }
' "$CM" > "$CM.new" || {
	rm -f "$CM.new"
	echo "$0: did not find libarchive_SOURCES in $CM" >&2
	exit 1
}
mv "$CM.new" "$CM"

# The frameworks archive_check_entitlement.c needs.
cat >> "$CM" <<'EOF'

# xnuports: Apple's archive_check_entitlement.c calls into Security to
# ask whether the caller may use a given filter.  Upstream's build has
# no reason to know about either framework.
FIND_LIBRARY(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
FIND_LIBRARY(SECURITY_FRAMEWORK Security)
IF(CORE_FOUNDATION_FRAMEWORK AND SECURITY_FRAMEWORK)
  TARGET_LINK_LIBRARIES(archive_static ${CORE_FOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK})
  IF(TARGET archive)
    TARGET_LINK_LIBRARIES(archive ${CORE_FOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK})
  ENDIF()
ENDIF()
EOF
