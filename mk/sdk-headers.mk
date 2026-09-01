# mk/sdk-headers.mk
#
# Populate the SDK's usr/include from the open-source releases the tree
# already carries.
#
# The bundles emitted by mk/bundle.mk are a shape with nothing in them;
# this puts the C and POSIX headers in, so the toolchain can compile
# against its own SDK rather than borrowing Xcode's.  Everything here
# comes from Apple's published sources at the versions in the macOS 26.5
# release manifest -- Libc, xnu, libpthread and their neighbours -- which
# is what makes it something this tree can ship.  Apple's own SDK headers
# are not redistributable and none are copied.
#
# What this cannot supply is the frameworks.  Foundation, AppKit and the
# rest have no open-source release, so an SDK built here compiles C and
# POSIX and stops there.
#
# The layout is the SDK's, not the sources': a project's headers land
# where a program expects to include them from, which is rarely how the
# project stores them.

TOP?=		${.CURDIR}

.include "${TOP}/mk/xcodetools.sys.mk"

RELEASE=	${TOP}/build/release
SDK_ROOT=	${RELEASE}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
SDK_INC=	${SDK_ROOT}/usr/include

LIBC=		${TOP}/lib/libc
XNU=		${TOP}/src/apple-oss-distributions/xnu
LIBPTHREAD=	${TOP}/lib/libpthread
LIBMALLOC=	${TOP}/lib/libmalloc
LIBCLOSURE=	${TOP}/lib/libclosure
LIBUTIL=	${TOP}/lib/libutil
COMMONCRYPTO=	${TOP}/lib/commoncrypto
COPYFILE=	${TOP}/src/apple-oss-distributions/copyfile
REMOVEFILE=	${TOP}/src/apple-oss-distributions/removefile
LIBDISPATCH=	${TOP}/lib/libdispatch

sdk-headers:
	@${ECHO} "sdk: installing headers into MacOSX.sdk/usr/include"
	@mkdir -p ${SDK_INC}/sys ${SDK_INC}/mach ${SDK_INC}/machine \
	    ${SDK_INC}/arm ${SDK_INC}/pthread ${SDK_INC}/malloc \
	    ${SDK_INC}/os ${SDK_INC}/dispatch ${SDK_INC}/libkern \
	    ${SDK_INC}/net ${SDK_INC}/netinet ${SDK_INC}/arpa \
	    ${SDK_INC}/uuid ${SDK_INC}/xpc ${SDK_INC}/CommonCrypto

	# xnu's shared headers go first.  Availability and the assert
	# macros come only from here, but so does a stdint.h meant for the
	# kernel -- laid down before the C library's, so that the one a
	# user program includes is Libc's.
	@cp -f ${XNU}/EXTERNAL_HEADERS/*.h ${SDK_INC}/ 2>/dev/null || true

	# The C library itself.  The subdirectories matter as much as the
	# top level: sys/_types holds the one-type-per-file headers that
	# every other header composes itself from, and without them
	# stdio.h has no definition of va_list.
	@cp -f ${LIBC}/include/*.h ${SDK_INC}/ 2>/dev/null || true
.for d in sys arpa malloc xlocale secure libkern protocols _types
	@mkdir -p ${SDK_INC}/${d}
	@cp -Rf ${LIBC}/include/${d}/. ${SDK_INC}/${d}/ 2>/dev/null || true
.endfor

	# The kernel's user-facing interfaces.
	@cp -f ${XNU}/bsd/sys/*.h ${SDK_INC}/sys/ 2>/dev/null || true
	# The syscall wrappers' own headers, which unistd.h includes.
	@cp -f ${XNU}/libsyscall/wrappers/*.h ${SDK_INC}/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/sys/_types ${SDK_INC}/mach/machine
	@cp -Rf ${XNU}/bsd/sys/_types/. ${SDK_INC}/sys/_types/ 2>/dev/null || true
	@cp -f ${XNU}/osfmk/mach/*.h ${SDK_INC}/mach/ 2>/dev/null || true
	@cp -Rf ${XNU}/osfmk/mach/machine/. ${SDK_INC}/mach/machine/ 2>/dev/null || true
	@cp -Rf ${XNU}/osfmk/mach/arm/. ${SDK_INC}/mach/arm/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/net/*.h ${SDK_INC}/net/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/netinet/*.h ${SDK_INC}/netinet/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/uuid/*.h ${SDK_INC}/uuid/ 2>/dev/null || true
	@cp -f ${XNU}/libkern/libkern/*.h ${SDK_INC}/libkern/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/libkern/arm
	@cp -f ${XNU}/libkern/libkern/arm/*.h ${SDK_INC}/libkern/arm/ 2>/dev/null || true

	# machine/ is the per-architecture indirection every sys header
	# reaches through; on this hardware it is arm.
	@cp -f ${XNU}/osfmk/arm/*.h ${SDK_INC}/arm/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/arm/*.h ${SDK_INC}/arm/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/machine/*.h ${SDK_INC}/machine/ 2>/dev/null || true
	@cp -f ${XNU}/osfmk/machine/*.h ${SDK_INC}/machine/ 2>/dev/null || true

	# Threads, allocation, blocks, and the rest.
	@cp -Rf ${LIBPTHREAD}/include/pthread/. ${SDK_INC}/pthread/ 2>/dev/null || true
	# pthread.h is inside the pthread/ directory in the source and at
	# the top level in an SDK; it is installed as both.
	@cp -f ${LIBPTHREAD}/include/pthread/pthread.h ${SDK_INC}/ 2>/dev/null || true
	@cp -Rf ${LIBPTHREAD}/include/sys/. ${SDK_INC}/sys/ 2>/dev/null || true
	@cp -f ${LIBMALLOC}/include/malloc/*.h ${SDK_INC}/malloc/ 2>/dev/null || true
	@cp -f ${LIBCLOSURE}/Block.h ${LIBCLOSURE}/Block_private.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${LIBUTIL}/*.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${COPYFILE}/copyfile.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${REMOVEFILE}/removefile.h ${SDK_INC}/ 2>/dev/null || true
	@cp -Rf ${COMMONCRYPTO}/include/. ${SDK_INC}/CommonCrypto/ 2>/dev/null || true
	@cp -Rf ${LIBDISPATCH}/dispatch/. ${SDK_INC}/dispatch/ 2>/dev/null || true
	@cp -Rf ${LIBDISPATCH}/os/. ${SDK_INC}/os/ 2>/dev/null || true

	# Two of xnu's headers are generated rather than shipped -- the
	# POSIX availability macros and the symbol-aliasing helpers -- which
	# is why copying the tree alone leaves sys/cdefs.h including files
	# that do not exist.
	#
	# The aliasing generator reads its list of OS versions by running
	# <sdk>/usr/local/libexec/availability.pl, which Apple ships only
	# in their internal SDK.  Ours goes to that path in our own SDK, so
	# the generator finds it where it expects to and this tree needs
	# nothing of Apple's to produce the header.
	@${ECHO} "sdk: generating xnu's derived headers"
	@mkdir -p ${SDK_ROOT}/usr/local/libexec
	@cp -f ${TOP}/mk/scripts/availability.pl ${SDK_ROOT}/usr/local/libexec/
	@chmod +x ${SDK_ROOT}/usr/local/libexec/availability.pl
	@sh ${XNU}/bsd/sys/make_posix_availability.sh \
	    ${SDK_INC}/sys/_posix_availability.h > /dev/null 2>&1 || true
	@sh ${XNU}/bsd/sys/make_symbol_aliasing.sh "${SDK_ROOT}" \
	    ${SDK_INC}/sys/_symbol_aliasing.h > /dev/null 2>&1 || true

	@${ECHO} "sdk: `find ${SDK_INC} -name '*.h' | wc -l | tr -d ' '` headers installed"

.PHONY: sdk-headers
