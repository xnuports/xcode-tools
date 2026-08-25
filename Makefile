# xcode-tools -- top-level Makefile (BSD bmake).
#
#	./configure?  No.  Just:  bmake
#
# Targets:
#	all		build every library and program into build/
#	ports		build components that carry their own build system
#			(MK_PORTS=yes; off by default, they are slow)
#	bundles		emit the .xctoolchain / .sdk bundle metadata
#	check		verify every inventory entry produced a binary
#	clean		remove build/ entirely
#	list-progs	print the tool inventory with release placements
#
# The release tree lands in build/release/ and is a drop-in replacement
# for /Applications/Xcode.app/Contents/Developer/ -- see mk/progs.mk.

TOP?=		${.CURDIR}

.include "${TOP}/mk/xcodetools.sys.mk"

RELEASE=	${TOP}/build/release

all: dirs lib progs ports bundles
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

ports:
	${MAKE} -C ${TOP}/ports TOP=${TOP}

bundles:
	${MAKE} -f ${TOP}/mk/bundle.mk TOP=${TOP} bundles

check:
	${MAKE} -C ${TOP}/src TOP=${TOP} check-progs
	${MAKE} -C ${TOP}/ports TOP=${TOP} check-ports

list-progs:
	${MAKE} -C ${TOP}/src TOP=${TOP} list-progs

list-ports:
	${MAKE} -C ${TOP}/ports TOP=${TOP} list-ports

clean:
	rm -rf ${TOP}/build

.PHONY: all dirs lib progs ports bundles check list-progs list-ports clean
