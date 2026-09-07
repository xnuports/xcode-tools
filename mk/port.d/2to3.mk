# 2to3.
#
# Xcode ships 2to3 and 2to3-3.9 in Developer/usr/bin, both console scripts
# over lib2to3.  CPython removed lib2to3 in 3.13, so the port is built from
# fissix, the maintained fork, with platformdirs alongside it because fissix
# asks it where to cache its grammar tables.  mk/scripts/install-2to3.sh
# does the work and says there why nothing is built rather than installed.
#
# Like pip, this has to follow python/cpython in PORTS: the version names
# the site-packages directory this writes into and the second script.  The
# awk is on one line because a "!=" command is not continued across
# backslash-newlines, and the "#" of each #define is escaped or it would
# start a comment.
#
# Where this differs from Apple's, measured over 149 files of CPython 3.9's
# own standard library, rewritten by both and compared:
#
#	147 of the 149 come out byte for byte identical.  The two that do
#	not are the same difference twice -- Apple's wraps zip() in list()
#	even inside dict(), where nothing needs a list, and fissix does
#	not.  Both are correct Python 3; fissix's is the tidier.
#
#	Apple's logs "No changes to <file>" for a file it did not touch and
#	then lists it under "Files that need to be modified" anyway.  fissix
#	fixed that, so it says neither.
#
#	"2to3 -l" and "-v" mention a fixer called "sorted" that fissix added
#	and lib2to3 never had.
#
# All three are places where fissix deliberately improved on lib2to3, and
# undoing them would mean shipping the bugs on purpose.  The one place it
# was behind rather than ahead -- a grammar with no room for positional-only
# parameters, which made it fail on files Apple's converts -- is fixed by
# mk/patches/fissix, applied to the installed copy and not to the submodule.
#
# Everything else checked matches: plain, -p, -e, -v, -f, -x, -d, -W,
# --add-suffix with --output-dir, and -h.
PY_VERSION!=	awk '/^\#define PY_MAJOR_VERSION/{maj=$$3} /^\#define PY_MINOR_VERSION/{min=$$3} END{print maj "." min}' ${TOP}/src/python/cpython/Include/patchlevel.h

P_COPY=			no
P_BUILDSYS=		make
P_MAKE=			sh ${TOP}/mk/scripts/install-2to3.sh
P_MAKE_ARGS=		${TOP}/src/python/fissix \
			${TOP}/src/python/platformdirs \
			${PY_VERSION} \
			${P_OBJDIR} \
			${TOP}/mk/patches/fissix
P_NOSTAGE=		yes

P_PROGS=		bin/2to3 bin/2to3-${PY_VERSION}
P_RELEASE_MERGE=	lib usr/lib
