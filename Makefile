# xcode-tools -- top-level Makefile (BSD bmake).
#
#	./configure?  No.  Just:  bmake
#
# Targets:
#	all		build every library and program into build/
# ports runs before progs on purpose: ld64 links against libtapi, which
# the llvm port stages, so the ports have to be in place first.
#	ports		build components that carry their own build system
#			(MK_PORTS=yes; off by default, they are slow)
#	bundles		emit the .xctoolchain / .sdk bundle metadata
#	check		verify every inventory entry produced a binary
#	clean		remove build/, keeping the ports work directories
#	clean-ports	remove the ports work directories
#	distclean	remove build/ entirely
#	list-progs	print the tool inventory with release placements
#
# The release tree lands in build/release/ and is a drop-in replacement
# for /Applications/Xcode.app/Contents/Developer/ -- see mk/progs.mk.

TOP?=		${.CURDIR}

.include "${TOP}/mk/xcodetools.sys.mk"

RELEASE=	${TOP}/build/release

all: dirs lib ports progs bundles sdk
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

# The SDK's headers, from the open-source releases in lib/ and src/.
# Runs after bundles, which creates the bundle the headers go into.
sdk:
	${MAKE} -f ${TOP}/mk/sdk-headers.mk TOP=${TOP} sdk

# Generates the headers xnu's build produces rather than ships, which
# the SDK's mach/ and Kernel.framework need.  Not part of `all': it
# wants the network, a Kernel Debug Kit and the better part of an
# hour.  Run it once and the SDK picks the results up from then on.
xnu-headers:
	${MAKE} -f ${TOP}/mk/sdk-headers.mk TOP=${TOP} xnu-headers

check:
	${MAKE} -C ${TOP}/src TOP=${TOP} check-progs
	${MAKE} -C ${TOP}/ports TOP=${TOP} check-ports

list-progs:
	${MAKE} -C ${TOP}/src TOP=${TOP} list-progs

list-ports:
	${MAKE} -C ${TOP}/ports TOP=${TOP} list-ports

# clean deliberately spares build/ports.  A port can cost hours -- the
# llvm port is most of an hour on ten cores -- and wiping that as part of
# an ordinary rebuild is not what anyone means by "clean".  Use
# clean-ports for that, or distclean for everything.
clean:
	find ${TOP}/build -mindepth 1 -maxdepth 1 ! -name ports -exec rm -rf {} + 2>/dev/null || true

clean-ports:
	rm -rf ${TOP}/build/ports

distclean: clean clean-ports
	rm -rf ${TOP}/build

.PHONY: all dirs lib progs ports bundles check xnu-headers \
	list-progs list-ports \
	clean clean-ports distclean
