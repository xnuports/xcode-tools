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
# One difference from Apple's worth knowing: "2to3 -l" lists one extra
# fixer, "sorted", which fissix added and lib2to3 never had.  Everything
# else -- the diffs, the log lines, the rewritten source -- matches Apple's
# byte for byte.
PY_VERSION!=	awk '/^\#define PY_MAJOR_VERSION/{maj=$$3} /^\#define PY_MINOR_VERSION/{min=$$3} END{print maj "." min}' ${TOP}/src/python/cpython/Include/patchlevel.h

P_COPY=			no
P_BUILDSYS=		make
P_MAKE=			sh ${TOP}/mk/scripts/install-2to3.sh
P_MAKE_ARGS=		${TOP}/src/python/fissix \
			${TOP}/src/python/platformdirs \
			${PY_VERSION} \
			${P_OBJDIR}
P_NOSTAGE=		yes

P_PROGS=		bin/2to3 bin/2to3-${PY_VERSION}
P_RELEASE_MERGE=	lib usr/lib
