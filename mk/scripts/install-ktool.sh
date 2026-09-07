#!/bin/sh
#
# Install ktool.
#
# A Python package with a console script, so this lays it down the way the
# pip and 2to3 ports lay theirs down: the packages into the interpreter's
# site-packages, the script into the toolchain's bin.  Four packages, not
# one -- ktool is the front end, ktool_macho and ktool_swift are the
# parsers, and lib0cyn is the shared support -- plus Pygments, which it
# highlights its output with.
#
# No dist-info directory, for the same reason 2to3 gets none: nothing is
# meant to upgrade these in place.
#
# Usage: install-ktool.sh <ktool-checkout> <pygments-checkout> <x.y> \
#	     <destdir> <patchdir>
#
set -e

KTOOL_SRC=$1
PYGMENTS_SRC=$2
PYVER=$3
DEST=$4
PATCHDIR=$5

if [ $# -ne 5 ]; then
	echo "usage: $0 <ktool-checkout> <pygments-checkout> <x.y>" \
	    "<destdir> <patchdir>" >&2
	exit 1
fi

# The same check the other two installers make: an empty version would put
# this where no interpreter looks, quietly.
case "${PYVER}" in
[0-9]*.[0-9]*) ;;
*)
	echo "$0: python version '${PYVER}' is not x.y" >&2
	exit 1
	;;
esac

SITE=${DEST}/lib/python${PYVER}/site-packages

rm -rf "${DEST}/bin" "${DEST}/lib"
mkdir -p "${SITE}" "${DEST}/bin"

# __pycache__ is left behind: it is the host's, keyed to a magic number
# that need not be the target's.
for pkg in ktool ktool_macho ktool_swift lib0cyn; do
	rsync -a --exclude '__pycache__' \
	    "${KTOOL_SRC}/src/${pkg}/" "${SITE}/${pkg}/"
done
rsync -a --exclude '__pycache__' \
    "${PYGMENTS_SRC}/pygments/" "${SITE}/pygments/"

# The patches, against the copy rather than the checkout -- the submodule
# stays read-only, the way ports here are patched in their work directory.
# --forward is not passed: a patch that no longer applies means ktool has
# changed under us and the reason for it needs looking at, not skipping.
for p in "${PATCHDIR}"/*.patch; do
	[ -e "${p}" ] || continue
	(cd "${SITE}" && patch -s -p1 < "${p}") || {
		echo "$0: failed to apply ${p}" >&2
		exit 1
	}
done

# The console script, from the entry point pyproject.toml declares.  The
# shebang is xcrun rather than a path, so it follows xcode-select the way
# every other tool here does.
cat > "${DEST}/bin/ktool" <<'SCRIPT'
#!/usr/bin/xcrun python3
# -*- coding: utf-8 -*-
import re
import sys
from ktool.ktool_script import main
if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
SCRIPT
chmod 755 "${DEST}/bin/ktool"

echo "ktool installed for python ${PYVER}"
