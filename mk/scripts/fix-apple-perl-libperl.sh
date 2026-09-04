#!/bin/sh
#
# fix-apple-perl-libperl.sh TREE
#
# Point perl's executables at the libperl.dylib they will actually
# find.
#
# perl builds libperl.dylib with -install_name set to wherever it sits
# in the build directory, links everything against that, and relies on
# installperl to rewrite the references afterwards -- which is what the
# -headerpad_max_install_names on every link is reserved for.  Its
# fix_dep_names() does the rewrite with
#
#	install_name_tool -change <getcwd>/libperl.dylib <archlib>/CORE/...
#
# and install_name_tool treats a -change whose old path does not appear
# in the binary as a no-op, exit status zero.  Under this build the cwd
# at install time is not the directory the recorded path names, so
# every executable keeps pointing into /tmp/<project>/Build and the
# install reports success.
#
# The dylib itself is fine: that fix is -id, which takes no old path.
# So the correct target is simply its install name, and everything in
# the tree that still refers to the build copy is pointed at it.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

TREE=$1
[ -n "$TREE" ] && [ -d "$TREE" ] || { echo "usage: $0 TREE" >&2; exit 1; }

LIBPERL=$(find "$TREE" -name 'libperl.dylib' -type f 2>/dev/null | head -1)
[ -n "$LIBPERL" ] || { echo "$0: no libperl.dylib under $TREE" >&2; exit 1; }

# Where it says it lives, which is where the executables must look.
NEWID=$(otool -D "$LIBPERL" 2>/dev/null | tail -1)
case "$NEWID" in
/System/Library/Perl/*) ;;
*)	echo "$0: libperl.dylib install name looks wrong: $NEWID" >&2
	exit 1 ;;
esac

fixed=0
for f in $(find "$TREE" -type f -perm -u+x 2>/dev/null); do
	case "$(file -b "$f" 2>/dev/null)" in
	*Mach-O*) ;;
	*)	continue ;;
	esac

	old=$(otool -L "$f" 2>/dev/null | awk '/libperl\.dylib/ {print $1; exit}')
	[ -n "$old" ] || continue
	[ "$old" = "$NEWID" ] && continue

	chmod u+w "$f"
	install_name_tool -change "$old" "$NEWID" "$f" 2>/dev/null || {
		echo "$0: could not rewrite $f" >&2
		exit 1
	}
	fixed=$((fixed + 1))
done

echo "    perl: pointed $fixed binaries at $NEWID"
