#!/bin/sh
#
# prepare-apple-perl.sh [DIR]
#
# Make Apple's perl buildable by someone who is not root.
#
# 5.34/GNUmakefile unpacks perl-5.34.1.tar.gz and then rewrites a dozen
# files in the unpacked tree -- appending to hints/darwin.sh, then a
# run of ed(1) scripts over Configure, doio.c and the rest.  The
# tarball carries those files mode 444, which stops none of it when the
# build runs as root, and stops all of it otherwise: the very first
# append fails with "Permission denied".
#
# So a chmod goes in after the unpack.  Nothing else about the rule
# changes, and the edits Apple make are left exactly as they are.
#
# The same thing happens again at the end.  Two rules pipe a sed script
# into "ex - <file>", and the second one's target has already been
# installed into DSTROOT, where installperl leaves it mode 444.  ex
# cannot write it and the install fails with no message of its own.
# Each such line gets a chmod ahead of the ex, in the same pipeline.
#
# And a third time in installstrip, which runs strip(1) over everything
# Mach-O in DSTROOT -- every .bundle installperl put there mode 444.
# That rule starts by cd'ing into DSTROOT, so a chmod goes in there.
#
# installstrip then does a second thing: cpio the unstripped binaries
# into SYMROOT and run dsymutil over each one, which is how Apple feed
# their symbol server.  None of it is installed and it does not survive
# outside their build -- the cpio fails on the symlinks perl installs
# ("Cannot extract through symlink"), and dsymutil then has nothing to
# read.  That half is dropped and the strip is kept, so the binaries
# that ship are stripped exactly as Apple's are.
#
# Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

set -e

DIR=${1:-.}
MF=$DIR/5.34/GNUmakefile

[ -f "$MF" ] || { echo "$0: no 5.34/GNUmakefile under $DIR" >&2; exit 1; }

# Already done -- this runs on a fresh copy each time, but be safe.
if grep -q 'chmod -R u+w' "$MF"; then
	exit 0
fi

awk '
	# The unpack rule: make the extracted tree writable before the
	# appends and ed(1) scripts that follow.
	/^[ \t]*mv \$\(PROJVERS\) \$\(PROJECT\) && \\$/ {
		print
		print "\tchmod -R u+w $(PROJECT) && \\"
		unpack = 1
		next
	}
	# installstrip: keep the strip, drop the symbol archiving.
	/^installstrip:[ \t]*$/ {
		print "installstrip:"
		print "\t@set -x && \\"
		print "\tcd $(DSTROOT) && \\"
		print "\tchmod -R u+w . && \\"
		print "\tfind . -type f | xargs file | fgrep '"'"'Mach-O'"'"' | grep -v '"'"'):[ \t]'"'"' | sed -e '"'"'s/: .*//'"'"' -e '"'"'s,^\\./,,'"'"' > $(SYMROOT)/strip.list && \\"
		print "\tstrip -x `cat $(SYMROOT)/strip.list`"
		instrip = 1
		skipping = 1
		next
	}
	skipping && /^[^ \t]/ { skipping = 0 }
	skipping { next }

	# "| ex - <file>": chmod the file in the same pipeline.
	/^[ \t]*\| ex - / {
		line = $0
		sub(/^[ \t]*\| ex - /, "", line)
		printf "\t| { chmod u+w %s 2>/dev/null || true; ex - %s; }\n", line, line
		exed++
		next
	}
	{ print }
	END {
		if (!unpack)  exit 1
		if (!exed)    exit 2
		if (!instrip) exit 3
	}
' "$MF" > "$MF.new" || {
	rc=$?
	rm -f "$MF.new"
	case $rc in
	1) echo "$0: did not find the unpack rule in $MF" >&2 ;;
	2) echo "$0: did not find an ex rule in $MF" >&2 ;;
	3) echo "$0: did not find the installstrip rule in $MF" >&2 ;;
	*) echo "$0: awk failed on $MF" >&2 ;;
	esac
	exit 1
}

mv "$MF.new" "$MF"
