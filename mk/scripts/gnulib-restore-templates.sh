#!/bin/sh
#
# gnulib-restore-templates.sh -- recreate missing gnulib "_.h" templates.
#
# The old gnulib bundled with gm4 and bison generates headers like
# lib/alloca.h and lib/getopt.h from templates named lib/alloca_.h and
# lib/getopt_.h:
#
#	alloca.h: alloca_.h
#		cp $(srcdir)/alloca_.h $@-t
#		mv $@-t $@
#
# Apple's drops ship the generated headers but not the templates, so the
# build stops at "No rule to make target 'alloca_.h'".  The two files are
# the same content, so the templates are restored from the headers that
# did ship.
#
# Run from the root of a copied port tree (see P_PREPARE in mk/port.mk).
# Never run against a submodule: this writes files.
#
# Copyright (c) 2026 Sunneva N. Mariu
# SPDX-License-Identifier: BSD-3-Clause

set -e

restored=0

for mf in lib/Makefile.in lib/Makefile.am; do
	[ -f "$mf" ] || continue

	# Rule lines of the form "foo.h: foo_.h" name every template the
	# build expects; take the base names from those.
	sed -n 's/^\([A-Za-z0-9_]*\)\.h:[ 	]*\1_\.h.*$/\1/p' "$mf" | sort -u |
	while read -r base; do
		[ -n "$base" ] || continue
		[ -f "lib/${base}_.h" ] && continue
		if [ -f "lib/${base}.h" ]; then
			cp "lib/${base}.h" "lib/${base}_.h"
			echo "  restored lib/${base}_.h from lib/${base}.h"
			restored=$((restored + 1))
		fi
	done
done

exit 0
