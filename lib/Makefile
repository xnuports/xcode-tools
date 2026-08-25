# lib/Makefile
#
# Builds the static libraries consumed by the tools, from submodule
# sources, into build/lib/.  Submodules are read-only; sources are
# referenced in place.
#
# Nothing is built yet: the ten tools in src/xcode/ have no shared
# library of their own.  cctools' libstuff and libmacho land here with
# stage 2a, at which point mk/with-libstuff.mk and mk/with-libmacho.mk
# let the cctools programs consume them.

TOP?=		${.CURDIR}/..

.include "${TOP}/mk/xcodetools.sys.mk"

OBJBASE=	${TOP}/build/lib

all:
	@${ECHO} "== no static libraries to build yet =="

clean:
	rm -rf ${OBJBASE}

list-libs:
	@${ECHO} "(none)"

.PHONY: all clean list-libs
