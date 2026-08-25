# xcode-tools -- top-level Makefile (BSD bmake).
#
#	./configure?  No.  Just:  bmake
#
# Targets:
#	all		build every library and program into build/
#	clean		remove build/ entirely
#	list-progs	print the tool inventory with release placements
#
# The release tree lands in build/release/ and is a drop-in replacement
# for /Applications/Xcode.app/Contents/Developer/ -- see mk/progs.mk.

TOP?=		${.CURDIR}

.include "${TOP}/mk/xcodetools.sys.mk"

RELEASE=	${TOP}/build/release

all: dirs lib progs
	@${ECHO} "== xcode-tools build complete =="
	@${ECHO} "   release tree: ${RELEASE}"

dirs:
.for d in usr/bin usr/lib usr/libexec usr/share ${XCTOOLCHAIN}/usr/bin Tools
	mkdir -p ${RELEASE}/${d}
.endfor

lib:
	${MAKE} -C ${TOP}/lib TOP=${TOP}

progs:
	${MAKE} -C ${TOP}/src TOP=${TOP}

list-progs:
	${MAKE} -C ${TOP}/src TOP=${TOP} list-progs

clean:
	rm -rf ${TOP}/build

.PHONY: all dirs lib progs list-progs clean
