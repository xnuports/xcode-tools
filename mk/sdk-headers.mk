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
SDK_FRM=	${SDK_ROOT}/System/Library/Frameworks
KERNEL_FW=	${SDK_FRM}/Kernel.framework
XNU_FAKEROOT=	${TOP}/tools/darwin-xnu-build/fakeroot

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
LLVM_BUILD=	${TOP}/build/ports/llvm/build
MSUN=		${TOP}/lib/msun

sdk-headers:
	@${ECHO} "sdk: installing headers into MacOSX.sdk/usr/include"
	@mkdir -p ${SDK_INC}/sys ${SDK_INC}/mach ${SDK_INC}/machine \
	    ${SDK_INC}/arm ${SDK_INC}/i386 ${SDK_INC}/pthread ${SDK_INC}/malloc \
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

	# Kernel.framework, which is what a kext compiles against and what
	# IOKit headers are found through.  Its headers are not a copy of
	# anything in xnu's tree either: the build assembles them, so they
	# come out of the same fakeroot.
	#
	# Apple's SDK carries Headers and nothing else -- no PrivateHeaders,
	# no Resources, no binary -- under the ordinary versioned layout, so
	# that is what gets installed.  Ours will be smaller than Apple's:
	# theirs also carries the driver families that come from DriverKit
	# and IOKitUser rather than from xnu.
.if exists(${XNU_FAKEROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers)
	@${ECHO} "sdk: installing Kernel.framework headers"
	@mkdir -p ${KERNEL_FW}/Versions/A/Headers
	@cp -Rf ${XNU_FAKEROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/. \
	    ${KERNEL_FW}/Versions/A/Headers/ 2>/dev/null || true
	@ln -sfn A ${KERNEL_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${KERNEL_FW}/Headers
.else
	@${ECHO} "sdk: no xnu fakeroot; Kernel.framework not installed"
.endif

	# TargetConditionals.h says which Apple platform is being compiled
	# for.  Almost everything Apple writes includes it eventually --
	# swift-foundation stops on it immediately -- and it ships in the
	# SDK's usr/include.  CoreFoundation is where the open source
	# carries it.
	@${TOP}/mk/scripts/emit-targetconditionals.sh \
	    > ${SDK_INC}/TargetConditionals.h

	# os/, whose headers come from two projects.  libplatform has the
	# public half of the locking primitives -- the private half was
	# already installed for ld64's sake -- and xnu's libkern/os has
	# the rest of what the SDK carries: base.h above all, which every
	# other os header includes for OS_ENUM and its neighbours.
	@mkdir -p ${SDK_INC}/os
	@cp -f ${TOP}/lib/libplatform/include/os/lock.h ${SDK_INC}/os/ \
	    2>/dev/null || true
.for h in base.h atomic.h overflow.h log.h trace.h object.h
	@cp -f ${XNU}/libkern/os/${h} ${SDK_INC}/os/ 2>/dev/null || true
.endfor

	# FreeBSD/ holds headers Libc took from there and installs flat.
	# nl_types.h is the one that matters: libc++ reaches for it
	# through <locale>, so without it no C++ program including
	# <vector> or <string> gets as far as being compiled.
	@cp -f ${LIBC}/include/FreeBSD/*.h ${SDK_INC}/ 2>/dev/null || true
.for d in sys arpa malloc xlocale secure libkern protocols _types
	@mkdir -p ${SDK_INC}/${d}
	@cp -Rf ${LIBC}/include/${d}/. ${SDK_INC}/${d}/ 2>/dev/null || true
.endfor

	# architecture/, which the Mach-O headers reach through for byte
	# order.
	@mkdir -p ${SDK_INC}/architecture
	@cp -Rf ${XNU}/EXTERNAL_HEADERS/architecture/. ${SDK_INC}/architecture/ 2>/dev/null || true

	# The Mach-O format headers.  Anything reading or writing a binary
	# includes these, and nothing else in the tree provides them.
	@mkdir -p ${SDK_INC}/mach-o
	@cp -Rf ${XNU}/EXTERNAL_HEADERS/mach-o/. ${SDK_INC}/mach-o/ 2>/dev/null || true

	# The kernel's user-facing interfaces.
	@cp -f ${XNU}/bsd/sys/*.h ${SDK_INC}/sys/ 2>/dev/null || true
	# The syscall wrappers' own headers, which unistd.h includes.
	@cp -f ${XNU}/libsyscall/wrappers/*.h ${SDK_INC}/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/sys/_types ${SDK_INC}/mach/machine
	@cp -Rf ${XNU}/bsd/sys/_types/. ${SDK_INC}/sys/_types/ 2>/dev/null || true
	@cp -f ${XNU}/osfmk/mach/*.h ${SDK_INC}/mach/ 2>/dev/null || true
	@cp -Rf ${XNU}/osfmk/mach/machine/. ${SDK_INC}/mach/machine/ 2>/dev/null || true
	@cp -Rf ${XNU}/osfmk/mach/arm/. ${SDK_INC}/mach/arm/ 2>/dev/null || true
	@cp -Rf ${XNU}/osfmk/mach/i386/. ${SDK_INC}/mach/i386/ 2>/dev/null || true

	# Headers xnu generates rather than ships, which must land after
	# the copies above and not before them.
	#
	# Much of mach/ is mig's output -- clock.h, mach_port.h, task.h,
	# mach_host.h and the rest come from libsyscall/mach/*.defs -- and
	# a few more are written by scripts in xnu's own build.  Copying
	# from the source tree cannot produce those at all.
	#
	# It also produces different, and correct, versions of headers
	# that do exist in osfmk/mach.  mach_interface.h is the one that
	# matters: the source tree's is the kernel's, and includes
	# clock_reply_server.h and the other mig *server* headers, which
	# no SDK ships and nothing in userland can resolve.  The installed
	# one is byte-identical to Apple's and includes none of them.  So
	# this runs last and the generated headers win; the source tree is
	# only the fallback for a tree with no fakeroot.
	#
	# tools/darwin-xnu-build produces it.  That needs the network, a
	# Kernel Debug Kit and the better part of an hour, so it is not
	# run from here: if the fakeroot is there its headers are taken,
	# and if it is not, this says so and carries on.  To make one:
	#
	#   bmake xnu-headers
	#
.if exists(${XNU_FAKEROOT}/usr/include/mach/clock.h)
	@${ECHO} "sdk: taking xnu's generated headers from darwin-xnu-build"
	@cp -Rf ${XNU_FAKEROOT}/usr/include/mach/. \
	    ${SDK_INC}/mach/ 2>/dev/null || true
.else
	@${ECHO} "sdk: no xnu fakeroot; mach/ will lack its generated headers"
.endif
	# mach/ headers that live outside osfmk/mach: vm_page_size.h is
	# libsyscall's, and everything asking the kernel about pages
	# reaches for it.
	@cp -f ${XNU}/libsyscall/mach/mach/vm_page_size.h ${SDK_INC}/mach/ \
	    2>/dev/null || true

	@mkdir -p ${SDK_INC}/mach_debug
	@cp -f ${XNU}/osfmk/mach_debug/*.h ${SDK_INC}/mach_debug/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/net/*.h ${SDK_INC}/net/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/netinet/*.h ${SDK_INC}/netinet/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/uuid/*.h ${SDK_INC}/uuid/ 2>/dev/null || true
	@cp -f ${XNU}/libkern/libkern/*.h ${SDK_INC}/libkern/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/libkern/arm ${SDK_INC}/libkern/i386
	@cp -f ${XNU}/libkern/libkern/arm/*.h ${SDK_INC}/libkern/arm/ 2>/dev/null || true
	@cp -f ${XNU}/libkern/libkern/i386/*.h ${SDK_INC}/libkern/i386/ 2>/dev/null || true

	# machine/ is the per-architecture indirection every sys header
	# reaches through, and it reaches through to whichever machine is
	# being compiled for rather than the one doing the compiling.  An
	# SDK carries the headers of every architecture it can build, so
	# both are installed: without i386 here, -arch x86_64 stops on
	# machine/_types.h before it has read anything else.
	@cp -f ${XNU}/osfmk/arm/*.h ${SDK_INC}/arm/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/arm/*.h ${SDK_INC}/arm/ 2>/dev/null || true
	@cp -f ${XNU}/osfmk/i386/*.h ${SDK_INC}/i386/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/i386/*.h ${SDK_INC}/i386/ 2>/dev/null || true
	@cp -f ${XNU}/bsd/machine/*.h ${SDK_INC}/machine/ 2>/dev/null || true
	@cp -f ${XNU}/osfmk/machine/*.h ${SDK_INC}/machine/ 2>/dev/null || true

	# Threads, allocation, blocks, and the rest.
	@cp -Rf ${LIBPTHREAD}/include/pthread/. ${SDK_INC}/pthread/ 2>/dev/null || true
	# pthread.h and sched.h are inside the pthread/ directory in the
	# source and at the top level in an SDK; they are installed as both.
	@cp -f ${LIBPTHREAD}/include/pthread/pthread.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${LIBPTHREAD}/include/pthread/sched.h ${SDK_INC}/ 2>/dev/null || true
	@cp -Rf ${LIBPTHREAD}/include/sys/. ${SDK_INC}/sys/ 2>/dev/null || true
	@cp -f ${LIBMALLOC}/include/malloc/*.h ${SDK_INC}/malloc/ 2>/dev/null || true
	@cp -f ${LIBCLOSURE}/Block.h ${LIBCLOSURE}/Block_private.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${LIBUTIL}/*.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${COPYFILE}/copyfile.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${REMOVEFILE}/removefile.h ${SDK_INC}/ 2>/dev/null || true
	@cp -Rf ${COMMONCRYPTO}/include/. ${SDK_INC}/CommonCrypto/ 2>/dev/null || true
	@cp -Rf ${LIBDISPATCH}/dispatch/. ${SDK_INC}/dispatch/ 2>/dev/null || true
	@cp -Rf ${LIBDISPATCH}/os/. ${SDK_INC}/os/ 2>/dev/null || true
	# libdispatch's os/ carries its build file; it is not a header and
	# nothing should find one in an SDK.  Removed after the copy that
	# brings it, not before.
	@rm -f ${SDK_INC}/os/CMakeLists.txt

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

	# xnu's availability macros stop at four platforms and current
	# headers call them with seven -- see the script.
	@${TOP}/mk/scripts/extend-availability.sh ${SDK_INC} || true

	# math.h, the one header here that is vendored rather than built.
	# Apple publishes no Libm, so there is nothing to build it from --
	# see lib/msun/README.md.  Only the declarations come from here;
	# the implementations are the system's, reached through the
	# libSystem stub at link time.
	@mkdir -p ${SDK_INC}/msun
	@cp -f ${MSUN}/math.h ${SDK_INC}/msun/ 2>/dev/null || true
	@cp -f ${MSUN}/math-darwin.h ${SDK_INC}/math.h 2>/dev/null || true

	# Select the platform in sys/cdefs.h.
	#
	# xnu's copy decides __DARWIN_ONLY_UNIX_CONFORMANCE and its
	# neighbours from XNU_PLATFORM_*, which xnu's own build defines and
	# a compiler does not.  With none of them set, arm64 falls to the
	# conformance-aliasing branch meant for 32-bit Intel, and stdio.h
	# declares fopen as fopen$UNIX2003 -- a symbol this system stopped
	# exporting long ago, so anything calling it compiles and then
	# fails to link.  Apple's SDK carries the already-selected header;
	# this selects it the same way, for the platform this SDK is.
	@printf '#define XNU_PLATFORM_MacOSX 1\n' > ${SDK_INC}/sys/cdefs.h.new
	@cat ${SDK_INC}/sys/cdefs.h >> ${SDK_INC}/sys/cdefs.h.new
	@mv ${SDK_INC}/sys/cdefs.h.new ${SDK_INC}/sys/cdefs.h

	# Libc's headers carry sections meant for building Libc itself,
	# which an SDK does not ship -- see mk/scripts/strip-libc-private.sh.
	@${TOP}/mk/scripts/strip-libc-private.sh ${SDK_INC} || true

	# The C++ standard library's headers, which an SDK carries at
	# usr/include/c++/v1 -- that is where Apple's are, not in the
	# toolchain.  Built by the llvm port as a runtime; without them
	# nothing including <string> compiles, however good the stub is.
	@if [ -d ${LLVM_BUILD}/include/c++/v1 ]; then \
		mkdir -p ${SDK_INC}/c++; \
		cp -Rf ${LLVM_BUILD}/include/c++/v1 ${SDK_INC}/c++/; \
		${ECHO} "sdk: libc++ headers installed"; \
	 else \
		${ECHO} "sdk: no libc++ headers (build the llvm port first)"; \
	 fi

	@${ECHO} "sdk: `find ${SDK_INC} -name '*.h' | wc -l | tr -d ' '` headers installed"

# --- stubs ----------------------------------------------------------
#
# The linker needs each library's exported symbols, not the library, and
# that is what a .tbd carries.  libSystem is the one every program links,
# and it is generated rather than copied: Apple's own stubs are part of
# their SDK and not redistributable, while the symbols themselves are
# read from the running system.
#
# See mk/scripts/make-tbd.sh for why they cannot simply be stubified out
# of /usr/lib with llvm-readtapi.

SDK_LIB=	${SDK_ROOT}/usr/lib

sdk-stubs:
	@${ECHO} "sdk: generating library stubs"
	@mkdir -p ${SDK_LIB}
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/libSystem.B.dylib \
	    ${SDK_LIB}/libSystem.B.tbd \
	    ${SDK_INC} || true
	@ln -sfn libSystem.B.tbd ${SDK_LIB}/libSystem.tbd
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/libc++.1.dylib \
	    ${SDK_LIB}/libc++.1.tbd \
	    ${SDK_INC} 2>/dev/null || true
	@[ -f ${SDK_LIB}/libc++.1.tbd ] && \
	    ln -sfn libc++.1.tbd ${SDK_LIB}/libc++.tbd || true
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/libobjc.A.dylib \
	    ${SDK_LIB}/libobjc.A.tbd \
	    ${SDK_INC} 2>/dev/null || true
	# libc++abi carries the personality routine every C++ program
	# references for exception unwinding, even one that throws nothing.
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/libc++abi.dylib \
	    ${SDK_LIB}/libc++abi.tbd \
	    ${SDK_INC} 2>/dev/null || true
	@[ -f ${SDK_LIB}/libobjc.A.tbd ] && \
	    ln -sfn libobjc.A.tbd ${SDK_LIB}/libobjc.tbd || true

# Builds xnu far enough to generate the headers the two blocks above
# take.  Kept out of sdk-headers because it wants the network, a Kernel
# Debug Kit and the better part of an hour; run once, then sdk-headers
# picks the results up on every build after.
xnu-headers:
	@${TOP}/mk/scripts/xnu-headers.sh

sdk: sdk-headers sdk-stubs

.PHONY: sdk sdk-headers sdk-stubs xnu-headers
