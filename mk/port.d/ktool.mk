# ktool -- reads Mach-O: load commands, symbols, the Objective-C and Swift
# metadata, and it can generate headers back out of a binary.
#
# The toolchain, not usr/local: by the rule mk/ports.mk states for
# src/extras, a tool that works on Mach-O belongs beside clang and ld.
#
# A Python package, so it installs rather than builds, and it has to follow
# python/cpython in PORTS for the same reason pip and 2to3 do: the version
# names the site-packages directory this writes into.  The awk is on one
# line because a "!=" command is not continued across backslash-newlines,
# and the "#" of each #define is escaped or it would start a comment.
PY_VERSION!=	awk '/^\#define PY_MAJOR_VERSION/{maj=$$3} /^\#define PY_MINOR_VERSION/{min=$$3} END{print maj "." min}' ${TOP}/src/python/cpython/Include/patchlevel.h

P_COPY=			no
P_BUILDSYS=		make
P_MAKE=			sh ${TOP}/mk/scripts/install-ktool.sh
P_MAKE_ARGS=		${TOP}/src/extras/ktool \
			${TOP}/src/extras/pygments \
			${PY_VERSION} \
			${P_OBJDIR} \
			${TOP}/mk/patches/ktool
P_NOSTAGE=		yes

# mk/patches/ktool moves it off pkg_resources, which CPython no longer
# installs and setuptools has removed; the patch says the rest.
P_PROGS=		bin/ktool
P_RELEASE_MERGE=	lib usr/lib
