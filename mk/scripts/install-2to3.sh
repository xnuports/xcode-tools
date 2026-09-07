#!/bin/sh
#
# Install 2to3 the way Xcode ships it.
#
# Xcode's 2to3 and 2to3-3.9 are five-line console scripts that hand the
# work to lib2to3, which came out of that Python's standard library.
# CPython removed lib2to3 in 3.13, so a tree building 3.14 has nothing to
# point them at.  fissix is that code, forked and maintained; the scripts
# here are Apple's, with the package name changed because it has to be.
#
# fissix is a normal third-party package with a dependency of its own, and
# both go into the same site-packages the pip port writes to.  Unlike pip,
# neither gets a dist-info directory: they stand in for a standard library
# module, which had none either, and nothing is meant to upgrade them.
#
# Usage: install-2to3.sh <fissix-checkout> <platformdirs-checkout> <x.y> \
#	     <destdir> <patchdir>
#
set -e

FISSIX_SRC=$1
PD_SRC=$2
PYVER=$3
DEST=$4
PATCHDIR=$5

if [ $# -ne 5 ]; then
	echo "usage: $0 <fissix-checkout> <platformdirs-checkout> <x.y>" \
	    "<destdir> <patchdir>" >&2
	exit 1
fi

# The same check the pip installer makes, for the same reason: the version
# names the site-packages directory and the second console script, and an
# empty one would install this where no interpreter looks, quietly.
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

# __pycache__ is left behind, as in the pip installer: it is the host's,
# keyed to a magic number that need not be the target's.
rsync -a --exclude '__pycache__' "${FISSIX_SRC}/fissix/" "${SITE}/fissix/"
rsync -a --exclude '__pycache__' "${PD_SRC}/src/platformdirs/" "${SITE}/platformdirs/"

# The patches, against the copy rather than the checkout -- the submodule
# stays read-only, the way ports here are patched in their work directory.
# --forward is not passed: a patch that no longer applies means fissix has
# changed under us and the reason for it needs looking at, not skipping.
for p in "${PATCHDIR}"/*.patch; do
	[ -e "${p}" ] || continue
	(cd "${SITE}" && patch -s -p1 < "${p}") || {
		echo "$0: failed to apply ${p}" >&2
		exit 1
	}
done

# platformdirs takes its version from the git tag, through hatch-vcs, which
# writes the file its __init__ imports.  We are not running hatchling, so
# the same file is written here from the same tag.  It is required rather
# than defaulted: a package that cannot say what version it is has been
# checked out in some way this script does not understand, and guessing
# would hide that.
PD_VERSION=$(git -C "${PD_SRC}" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
case "${PD_VERSION}" in
[0-9]*.[0-9]*)
	;;
*)
	echo "$0: cannot read platformdirs version from ${PD_SRC}" >&2
	exit 1
	;;
esac
PD_TUPLE=$(echo "${PD_VERSION}" | tr '.' ' ' | tr -s ' ' | sed 's/ /, /g')
cat > "${SITE}/platformdirs/version.py" <<EOF
# Written by mk/scripts/install-2to3.sh, standing in for hatch-vcs.
__version__ = version = '${PD_VERSION}'
__version_tuple__ = version_tuple = (${PD_TUPLE})
EOF

# The console scripts.  Apple's two are the same five lines under both
# names, and so are these; the shebang is xcrun rather than a path so they
# follow xcode-select the way every other tool here does.
for name in 2to3 "2to3-${PYVER}"; do
	cat > "${DEST}/bin/${name}" <<'SCRIPT'
#!/usr/bin/xcrun python3
import sys
from fissix.main import main

sys.exit(main("fissix.fixes"))
SCRIPT
	chmod 755 "${DEST}/bin/${name}"
done

echo "2to3 installed for python ${PYVER} (fissix, platformdirs ${PD_VERSION})"
