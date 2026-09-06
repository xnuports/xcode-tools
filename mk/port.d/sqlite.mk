# SQLite, upstream.
#
# Apple ship libsqlite3.dylib and sqlite3.h and do not publish the source:
# there is no sqlite in apple-oss-distributions, and the SOURCE_ID in their
# header ends "apl", which is their own build.  What their header does say is
# the version -- 3.51.0 -- so that is the tag here, and this is upstream's
# tree at it.
#
# Their header is not upstream's either.  It renames the include guard, adds
# a hundred and twenty availability annotations, and declares nineteen fewer
# functions.  The annotations and the guard cannot be reproduced from source
# and are not attempted.  The declarations can: six of the nineteen reach a
# translation unit, and every one of them would compile here and then fail to
# link against the system's libsqlite3, so they are removed.  The other
# thirteen are already behind a feature macro and never appear.
#
# Only the headers are built.  The library is the OS's and the SDK carries a
# stub for it; sqlite3.h is generated from src/sqlite.h.in by a tcl script in
# the tree, which is the only reason there is a build step at all.

# autosetup's configure, which does not know the autotools flags port.mk
# otherwise adds.
P_NO_AUTOTOOLS_FLAGS=	yes

# This tree drives ports with "bmake ... TOP=<repo>", and a command-line
# variable rides into every child make through MAKEFLAGS, where GNU make
# treats it as an override.  sqlite's own Makefile uses TOP for its source
# root, so ours replaced it and the build went looking for main.mk in this
# repository.  Dropping MAKEFLAGS hands the child its own variables back.
# Nothing here wants our make flags inside someone else's build anyway; any
# other port whose makefile uses TOP would hit the same thing.
P_MAKE=		env -u MAKEFLAGS make

P_MAKE_ARGS=	sqlite3.h
P_NOSTAGE=	yes

# Nothing but headers.
P_PROGS=

# sqlite3ext.h is a plain source file and needs no generating; it travels
# beside sqlite3.h because Apple ship both.
SQLITE_UNEXPORTED=	sqlite3_unlock_notify \
			sqlite3_db_status64 \
			sqlite3_set_errmsg \
			sqlite3_win32_set_directory \
			sqlite3_win32_set_directory8 \
			sqlite3_win32_set_directory16

P_POST_BUILD=	mkdir -p dest/usr/include && \
		cp -f sqlite3.h src/sqlite3ext.h dest/usr/include/ && \
		${TOP}/mk/scripts/prune-sqlite-header.sh \
		    dest/usr/include/sqlite3.h ${SQLITE_UNEXPORTED}

SDKS=		Platforms/MacOSX.platform/Developer/SDKs

P_RELEASE_MERGE=	dest/usr/include ${SDKS}/MacOSX.sdk/usr/include \
			dest/usr/include ${SDKS}/MacOSX.Internal.sdk/usr/include
