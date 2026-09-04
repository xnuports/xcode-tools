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
INTERNAL_SDK=	${RELEASE}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.Internal.sdk
INTERNAL_SPI=	${TOP}/src/apple-internals/apple_internal_sdk
APPLE_SPI=	${TOP}/lib/apple-spi
SDK_FRM=	${SDK_ROOT}/System/Library/Frameworks
INTERNAL_KERNEL_FW=	${INTERNAL_SDK}/System/Library/Frameworks/Kernel.framework
CF_SRC=		${TOP}/src/apple/CF
CF_EXTRA=	${TOP}/lib/cf-extra
CFNET_SRC=	${TOP}/src/apple/CFNetwork/Headers
CFNET_FW=	${SDK_FRM}/CFNetwork.framework
CF_FW=		${SDK_FRM}/CoreFoundation.framework
CORESERVICES=	${TOP}/lib/coreservices
SECURITY=	${TOP}/src/apple/Security
SEC_FW=		${SDK_FRM}/Security.framework
FOUNDATION_FW=	${SDK_FRM}/Foundation.framework
CS_FW=		${SDK_FRM}/CoreServices.framework
FSE_FW=		${CS_FW}/Versions/A/Frameworks/FSEvents.framework
KERNEL_FW=	${SDK_FRM}/Kernel.framework
XNU_FAKEROOT=	${TOP}/tools/darwin-xnu-build/fakeroot

LIBC=		${TOP}/src/apple/libc
LIBC_EXTRA=	${TOP}/lib/libc-extra
XPC_HEADERS=	${TOP}/lib/xpc
OBJC4=		${TOP}/src/apple/objc4
LIBINFO=	${TOP}/src/apple/libinfo
LIBPLATFORM=	${TOP}/src/apple/libplatform
EXPAT=		${TOP}/src/apple/expat
SYSLOG=		${TOP}/src/apple/syslog
BZIP2=		${TOP}/src/apple/bzip2
LIBPTHREAD=	${TOP}/src/apple/libpthread
LIBICONV=	${TOP}/src/apple/libiconv
ZLIB=		${TOP}/src/apple/zlib
CURL=		${TOP}/src/apple/curl
LIBEDIT=	${TOP}/src/apple/libedit
XNU=		${TOP}/src/apple/xnu
LIBPTHREAD=	${TOP}/src/apple/libpthread
LIBMALLOC=	${TOP}/src/apple/libmalloc
LIBCLOSURE=	${TOP}/src/apple/libclosure
LIBUTIL=	${TOP}/src/apple/libutil
COMMONCRYPTO=	${TOP}/src/apple/commoncrypto
COPYFILE=	${TOP}/src/apple/copyfile
REMOVEFILE=	${TOP}/src/apple/removefile
LIBDISPATCH=	${TOP}/src/apple/libdispatch
LLVM_BUILD=	${TOP}/build/ports/llvm/build
MSUN=		${TOP}/lib/msun
DYLD=		${TOP}/src/apple/dyld
SWIFT_LIB=	${TOP}/build/ports/swift/build/lib/swift/macosx
SWIFT_SHIMS=	${TOP}/build/ports/swift/build/lib/swift/shims
SWIFTC=		${TOP}/build/ports/swift/build/bin/swiftc
GYB=		${TOP}/src/swiftlang-llvm/swift/utils/gyb
SWIFT_APINOTES=	${TOP}/src/swiftlang-llvm/swift/apinotes
DARWIN_OVERLAY=	${TOP}/lib/swift-darwin-overlay
OS_OVERLAY=	${TOP}/lib/swift-os-overlay
OVERLAY_OBJ=	${TOP}/build/obj/swift-darwin-overlay
# The deployment target for the overlay's triple.  bundle.mk derives
# the same value the same way; it is not in xcodetools.sys.mk, so
# asking for it here without this gets an empty string and a triple
# of "arm64-apple-macosx", which swiftc rejects.
XT_SDK_VERSION!=	xcrun --show-sdk-version 2>/dev/null || echo 0.0
XT_DEPLOYMENT_TARGET?=	${XT_SDK_VERSION}
SDK_SWIFT=	${SDK_ROOT}/usr/lib/swift

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
	# Two Availability headers are not in EXTERNAL_HEADERS at all:
	# AvailabilityVersions.h is generated during the xnu build and
	# AvailabilityInternalLegacy.h ships beside it.  They are added
	# here; the three EXTERNAL_HEADERS does carry are left alone.
	#
	# Taking all five from the fakeroot instead does make
	# os/availability.h's API_AVAILABLE(macos(...)) expand, and breaks
	# sys/qos.h in exchange -- that header is xnu's own and is written
	# against EXTERNAL_HEADERS' macros, so it stops compiling and
	# Darwin stops building with it.  The two sets are not
	# interchangeable and the fakeroot has no sys/qos.h to match.
	# Until that is resolved coherently the source tree's three stay,
	# which keeps Darwin building; os.log is what pays for it.
.if exists(${XNU_FAKEROOT}/usr/include/AvailabilityVersions.h)
.for h in AvailabilityVersions.h AvailabilityInternalLegacy.h
	@cp -f ${XNU_FAKEROOT}/usr/include/${h} ${SDK_INC}/ 2>/dev/null || true
.endfor
.endif
	# xnu's Availability headers name five platforms -- ios, macos,
	# macosx, tvos and watchos -- and headers here annotate for more:
	# mach-o/dyld.h says __API_UNAVAILABLE(bridgeos) and will not
	# compile without it.  The rest are appended to the installed
	# AvailabilityInternal.h, which is where they have to be, since
	# dyld.h includes <Availability.h> and nothing else.
	# Appended, not included -- see the header.  Guarded because this
	# SDK is assembled incrementally: without the check every build
	# adds another copy, and once a stale copy is in place its include
	# guard swallows the new one, so edits stop taking effect.
	@grep -q __XNUPORTS_AVAILABILITY_PLATFORMS__ ${SDK_INC}/AvailabilityInternal.h 2>/dev/null || \
	    cat ${LIBC_EXTRA}/availability-platforms.h >> ${SDK_INC}/AvailabilityInternal.h 2>/dev/null || true

	# The C library itself.  The subdirectories matter as much as the
	# top level: sys/_types holds the one-type-per-file headers that
	# every other header composes itself from, and without them
	# stdio.h has no definition of va_list.
	@cp -f ${LIBC}/include/*.h ${SDK_INC}/ 2>/dev/null || true
	# Public headers Apple omits from its open-source drops, ours,
	# from lib/libc-extra.  sysdir.h is the one FileManager needs and
	# MacTypes.h is what OSStatus lives in; neither submodule can be
	# edited to add them.
	@cp -f ${LIBC_EXTRA}/*.h ${SDK_INC}/ 2>/dev/null || true

	# libplatform's public headers.  Libc does not carry these -- the
	# project owns setjmp.h, ucontext.h, the OSAtomic family and the
	# os/ locking headers -- and without them the SDK has no setjmp.h
	# at all, which stops anything including <CoreFoundation/...>.
	# exclavekit/ is deliberately skipped: Apple ships none of it.
	@cp -f ${LIBPLATFORM}/include/*.h ${SDK_INC}/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/os ${SDK_INC}/libkern
	@cp -f ${LIBPLATFORM}/include/os/*.h ${SDK_INC}/os/ 2>/dev/null || true
	# NB: libplatform's libkern/ headers are installed further down --
	# libkern is replaced wholesale from the fakeroot after this point.

	# XPC's public headers.  Apple publishes no source for libxpc and
	# no open-source release of its headers, but the implementation is
	# in libSystem -- 547 exported symbols -- so what is missing is
	# only the declarations.  lib/xpc says why they are written rather
	# than taken from one of the reimplementations.
	@mkdir -p ${SDK_INC}/xpc
	@cp -f ${XPC_HEADERS}/xpc/*.h ${SDK_INC}/xpc/ 2>/dev/null || true

	# Swift's API notes.  These are what rename the C spellings to the
	# ones Swift code actually writes -- os_log_type_t becomes
	# OSLogType, and without the file that type simply does not exist
	# as far as the compiler is concerned.  Apple keeps them at the
	# top of usr/include; the swift tree carries the two that matter.
.for f in os.apinotes Dispatch.apinotes
	@cp -f ${SWIFT_APINOTES}/${f} ${SDK_INC}/ 2>/dev/null || true
.endfor
	# And ours, for the clock ids -- see the file's own header.
	@cp -f ${LIBC_EXTRA}/_DarwinFoundation2.apinotes ${SDK_INC}/ 2>/dev/null || true

	# Libinfo's public headers.  The user and group database, the
	# resolver and the interface-address walker all live here rather
	# than in Libc, and the SDK had none of them -- FileManager and
	# anything calling getpwuid stopped at "cannot find 'passwd'".
.for h in lookup.subproj/pwd.h lookup.subproj/grp.h \
	  lookup.subproj/netdb.h lookup.subproj/aliasdb.h \
	  lookup.subproj/bootparams.h gen.subproj/ifaddrs.h
	@cp -f ${LIBINFO}/${h} ${SDK_INC}/ 2>/dev/null || true
.endfor

	# libiconv's public header.  Apple ships iconv.h and a libiconv
	# stub in the SDK; this tree had neither, which is how building
	# Apple's own git against this SDK stopped at "'iconv.h' file not
	# found".  The implementation is /usr/lib/libiconv.2.dylib rather
	# than libSystem, so it wants a stub of its own -- see sdk-stubs.
	@cp -f ${LIBICONV}/citrus/iconv.h ${SDK_INC}/ 2>/dev/null || true

	# The other libraries Apple ships headers for and this tree did
	# not.  Each is a separate open-source release and a separate
	# system dylib, so each needs its header here and its stub in
	# sdk-stubs.  Building Apple's own git against this SDK is what
	# found them, one at a time: iconv.h, then zlib.h, then the rest.
	@cp -f ${ZLIB}/zlib/zlib.h ${ZLIB}/zlib/zconf.h ${SDK_INC}/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/curl
	@cp -f ${CURL}/curl/include/curl/*.h ${SDK_INC}/curl/ 2>/dev/null || true
	# bzlib.h, which perl's Compress::Bzip2 wants.
	@cp -f ${BZIP2}/bzip2/bzlib.h ${SDK_INC}/ 2>/dev/null || true

	# asl.h, from Apple's syslog.  ASL is deprecated but the symbols are
	# still in libSystem and Apple's own perl still logs through it.
	# Only asl.h.  The drop carries a dozen more -- asl_core.h,
	# asl_msg.h, asl_private.h and so on -- and Apple's public SDK
	# ships none of them.
	@cp -f ${SYSLOG}/libsystem_asl.tproj/include/asl.h ${SDK_INC}/ 2>/dev/null || true

	# Two of libsyscall's headers that Apple ships in usr/include and
	# nothing here was installing: libproc.h, which Apple's perl
	# includes, and spawn.h.  They come from the fakeroot rather than
	# xnu's source tree because the source copies are the kernel's
	# build of them.
.for h in libproc.h spawn.h
	@cp -f ${XNU_FAKEROOT}/usr/include/${h} ${SDK_INC}/ 2>/dev/null || true
.endfor

	# libDER, which xnu vendors in EXTERNAL_HEADERS and Apple ships in
	# usr/include.  Apple installs two of the eleven headers there --
	# the type and the config -- and that is the set Security's oids.h
	# reaches for, so it is the set installed here.
	@mkdir -p ${SDK_INC}/libDER
.for h in DERItem.h libDER_config.h
	@cp -f ${XNU}/EXTERNAL_HEADERS/libDER/${h} ${SDK_INC}/libDER/ 2>/dev/null || true
.endfor
	@printf 'module libDER [system] {\n\
    umbrella "."\n\
\n\
    module * { export * }\n\
}\n' > ${SDK_INC}/libDER/module.modulemap

	# expat: the dylib is already stubbed below, and Apple ships the
	# header in usr/include, but nothing installed it until now.
	@cp -f ${EXPAT}/expat/lib/expat.h ${EXPAT}/expat/lib/expat_external.h \
	    ${SDK_INC}/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/editline
	@cp -f ${LIBEDIT}/src/histedit.h ${SDK_INC}/ 2>/dev/null || true
	@cp -f ${LIBEDIT}/src/editline/readline.h ${SDK_INC}/editline/ 2>/dev/null || true

	# The Objective-C runtime's public headers.  os/object.h includes
	# objc/NSObject.h once OS_OBJECT_USE_OBJC is on, which is what the
	# os module now reaches, and the SDK had no objc/ at all.  objc4's
	# runtime/ carries sixteen of the seventeen Apple ships; List.h is
	# a legacy header that is not in the open-source drop.
	@mkdir -p ${SDK_INC}/objc
.for h in NSObjCRuntime.h NSObject.h Object.h Protocol.h hashtable.h \
	  hashtable2.h message.h objc-api.h objc-auto.h objc-class.h \
	  objc-exception.h objc-load.h objc-runtime.h objc-sync.h \
	  objc.h runtime.h
	@cp -f ${OBJC4}/runtime/${h} ${SDK_INC}/objc/ 2>/dev/null || true
.endfor

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
	@cp -f ${TOP}/src/apple/libplatform/include/os/lock.h ${SDK_INC}/os/ \
	    2>/dev/null || true
.for h in base.h atomic.h overflow.h log.h trace.h object.h
	@cp -f ${XNU}/libkern/os/${h} ${SDK_INC}/os/ 2>/dev/null || true
.endfor
	# os/availability.h is the os/-namespace half of the availability
	# macros -- API_AVAILABLE and friends -- that os/object.h and thus
	# os/log.h include.  It is not in xnu's libkern/os; it comes out of
	# the fakeroot, so it is taken from there when there is one.
	# os/availability.h is ours, from lib/libc-extra: it defines the
	# public API_* spellings over the __API_* forms the source tree's
	# Availability headers provide.  The fakeroot has one, but it is
	# written against the fakeroot's Availability generation, which
	# cannot be installed here without breaking sys/qos.h.
	@cp -f ${LIBC_EXTRA}/os/availability.h ${SDK_INC}/os/ 2>/dev/null || true
	# Libc's own os/ headers, which Apple's public SDK does not carry
	# -- they are SPI.  os/assumes.h is the one Apple's git wants, and
	# this is a developer SDK for a toolset meant to use private
	# interfaces, so all six go in.
	@cp -f ${LIBC}/os/*.h ${SDK_INC}/os/ 2>/dev/null || true
	# assumes.h includes os/log_private.h, which is not Libc's: the
	# userland one comes from libtrace, which Apple does not publish,
	# and xnu's libkern carries a copy.  base_private.h and
	# feature_private.h come from the internal-SDK reconstruction,
	# which is where the headers Apple publishes no source for live.
	# Named one by one rather than copying libkern/os wholesale --
	# most of what is in there is the kernel's, and adding headers to
	# this SDK has twice been enough to disturb the module map.
	# NB: xnu's libkern/os/log_private.h is the *kernel* variant.  The
	# userspace one is libtrace's and is not published.  Installing the
	# kernel copy into usr/include only misleads -- it has none of the
	# os_log_pack ABI userspace callers want -- so it stays in
	# Kernel.framework, where the fakeroot already puts it.
.for h in base_private.h feature_private.h
	@cp -f ${INTERNAL_SPI}/usr/include/os/${h} ${SDK_INC}/os/ 2>/dev/null || true
.endfor
.if exists(${XNU_FAKEROOT}/usr/include/os/availability.h)
	# base.h, atomic.h and overflow.h from xnu's libkern above are the
	# kernel's, and the kernel's base.h does not pull in API_AVAILABLE
	# -- os/object.h uses it and does not compile against it.  The
	# fakeroot's are the userland versions, byte-identical to Apple's,
	# so they replace the kernel copies.
.for h in base.h atomic.h overflow.h
	@cp -f ${XNU_FAKEROOT}/usr/include/os/${h} ${SDK_INC}/os/ 2>/dev/null || true
.endfor
.endif

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

	# The dynamic loader's headers.  dlfcn.h is the one everything
	# wants -- dlopen and dlsym live nowhere else, and it is what
	# swift-foundation stops on -- and the four mach-o headers are
	# the rest of what dyld contributes to the SDK.
	#
	# lib/dyld/include carries fourteen headers and Apple's SDK ships
	# these five.  The other nine are SPI: dyld_priv.h, dlfcn_private.h,
	# the cache-format and introspection headers.  They are named one
	# by one rather than copied wholesale so the SDK keeps carrying
	# what Apple's carries and not what a source release happens to
	# have next to it.
	@cp -f ${DYLD}/include/dlfcn.h ${SDK_INC}/ 2>/dev/null || true
.for h in dyld.h dyld_images.h fixup-chains.h utils.h
	@cp -f ${DYLD}/include/mach-o/${h} ${SDK_INC}/mach-o/ 2>/dev/null || true
.endfor
	# dyld.h uses DYLD_EXCLAVEKIT_UNAVAILABLE thirty-eight times and
	# never defines it.  Apple's copy defines it, empty, inside an
	# "#ifndef __OPEN_SOURCE__" block -- and the open-source drop
	# strips that block while keeping every use, so the header as
	# published does not compile.  Apple's expansion is nothing, so
	# that is what goes in, ahead of the first use.
	@if ! grep -q 'define DYLD_EXCLAVEKIT_UNAVAILABLE' ${SDK_INC}/mach-o/dyld.h 2>/dev/null; then \
	    { ${ECHO} '/* Supplied by sdk-headers: stripped from the open-source drop. */'; \
	      ${ECHO} '#define DYLD_EXCLAVEKIT_UNAVAILABLE'; \
	      cat ${SDK_INC}/mach-o/dyld.h; } > ${SDK_INC}/mach-o/dyld.h.new && \
	    mv -f ${SDK_INC}/mach-o/dyld.h.new ${SDK_INC}/mach-o/dyld.h; \
	fi

	# The kernel's user-facing interfaces.
	@cp -f ${XNU}/bsd/sys/*.h ${SDK_INC}/sys/ 2>/dev/null || true
	# The audit interfaces.  bsm/audit.h reaches the SDK through
	# sys/ types that several headers pull in, and swift-foundation's
	# shims include it.  xnu carries nine of the twelve headers
	# Apple's SDK has here; audit_filter.h, audit_session.h and
	# libbsm.h come from the separate libbsm project, which is not in
	# this tree.  audit_kernel.h is xnu's alone and Apple ships none
	# of it, so the installed set is taken from the fakeroot below
	# where there is one.
	# IPv6.  netinet/in.h includes <netinet6/in6.h>, so without this
	# directory netinet/ does not compile at all -- which the module
	# map turned up, since nothing had tried to build a module over
	# these headers before.  xnu carries 28 and Apple's SDK ships 9;
	# the fakeroot's installed set is the nine, so it wins below.
	@mkdir -p ${SDK_INC}/netinet6
	@cp -f ${XNU}/bsd/netinet6/*.h ${SDK_INC}/netinet6/ 2>/dev/null || true
.if exists(${XNU_FAKEROOT}/usr/include/netinet6/in6.h)
	@rm -rf ${SDK_INC}/netinet6
	@mkdir -p ${SDK_INC}/netinet6
	@cp -Rf ${XNU_FAKEROOT}/usr/include/netinet6/. ${SDK_INC}/netinet6/ 2>/dev/null || true
.endif

	@mkdir -p ${SDK_INC}/bsm
	@cp -f ${XNU}/bsd/bsm/*.h ${SDK_INC}/bsm/ 2>/dev/null || true
	@rm -f ${SDK_INC}/bsm/audit_kernel.h
.if exists(${XNU_FAKEROOT}/usr/include/bsm/audit.h)
	@cp -Rf ${XNU_FAKEROOT}/usr/include/bsm/. ${SDK_INC}/bsm/ 2>/dev/null || true
.endif
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
	# complex.h likewise.  clang's own tgmath.h includes it with no
	# guard, so its absence stopped anything reaching <tgmath.h> --
	# swift-foundation's _CStdlib.h among them -- not just code
	# using complex arithmetic.
	@cp -f ${MSUN}/complex.h ${SDK_INC}/msun/ 2>/dev/null || true
	@cp -f ${MSUN}/complex-darwin.h ${SDK_INC}/complex.h 2>/dev/null || true

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
	# net/, netinet/ and machine/ are replaced outright rather than
	# copied over.  xnu's source tree carries the kernel's headers as
	# well as the ones that ship -- 90 in net/ where Apple installs
	# 18, 57 in netinet/ against 22, 56 in machine/ against 14 -- and
	# the surplus does not compile in userland: vm/vm_protos.h,
	# MARK_AS_FIXUP_TEXT and KCNumKinds are kernel-only and the
	# module map is the first thing here that ever tried to build
	# them.  The fakeroot's installed set is exactly Apple's for
	# these, so it replaces ours rather than merging with it.
	#
	# Two more go by name.  mach/dyld_kernel_fixups.h and
	# libkern/section_keywords.h are the kernel's own -- Apple ships
	# neither -- and they are what MARK_AS_FIXUP_TEXT and KCNumKinds
	# come from.
.for d in net netinet machine libkern
	@rm -rf ${SDK_INC}/${d}
	@mkdir -p ${SDK_INC}/${d}
	@cp -Rf ${XNU_FAKEROOT}/usr/include/${d}/. ${SDK_INC}/${d}/ 2>/dev/null || true
.endfor
	# The fakeroot's libkern is xnu's, and the OSAtomic family is not
	# xnu's -- it belongs to libplatform.  Apple ships both in
	# usr/include/libkern, so put libplatform's back now that the
	# wholesale replacement above is done.
	@cp -f ${LIBPLATFORM}/include/libkern/*.h ${SDK_INC}/libkern/ 2>/dev/null || true
	# mach/ comes from the fakeroot's two userland trees, not from
	# xnu's source.  The source tree's headers are the kernel's build
	# of them: its mach/message.h and mach/vm_param.h include
	# os/base.h and os/overflow.h, which Apple's do not, and that one
	# difference makes Darwin depend on the os module -- and os
	# already depends on Darwin, so nothing that spans both can be
	# built.
	#
	# usr/include/mach holds 27 headers and
	# System.framework/PrivateHeaders/mach another 75.  Together they
	# are a superset of Apple's 88 exactly: every one Apple ships,
	# plus fourteen kernel and private ones, which are kept.
	#
	# They were removed at first, for parity with Apple's public SDK,
	# and that was the wrong call: the task_policy.h and
	# thread_policy.h in this set include their _private siblings
	# where Apple's do not, so pruning them left public headers that
	# could not compile, one surfacing after another.  This is a
	# developer SDK for a toolset that is allowed to use private
	# interfaces; carrying more than Apple's public SDK is the point,
	# not a defect.  The module map declares only what compiles, so
	# the extras cost nothing there.
	@rm -rf ${SDK_INC}/mach
	@mkdir -p ${SDK_INC}/mach
	@cp -Rf ${XNU_FAKEROOT}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/mach/. \
	    ${SDK_INC}/mach/ 2>/dev/null || true
	@cp -Rf ${XNU_FAKEROOT}/usr/include/mach/. ${SDK_INC}/mach/ 2>/dev/null || true
	@mkdir -p ${SDK_INC}/mach/machine
	@cp -Rf ${XNU}/osfmk/mach/machine/. ${SDK_INC}/mach/machine/ 2>/dev/null || true
	# mig writes a *_server.h beside each interface -- the server half,
	# for a program implementing the call rather than making it.  An
	# SDK has no use for them and Apple ships none.
	@rm -f ${SDK_INC}/mach/*_server.h
	# libdispatch builds for more than Darwin, and its other platforms'
	# headers came along with the copy.  os/generic_win_base.h is the
	# one that bites: it typedefs mode_t to int and uses __declspec,
	# so anything building a module over os/ collides with the real
	# sys/_types.  Apple ships neither, nor the build-system files
	# beside them.
	@rm -f ${SDK_INC}/os/generic_win_base.h ${SDK_INC}/os/generic_unix_base.h
	# The os/log.h installed above is xnu's, which is the kernel's: it
	# has os_log_t and os_log_create but not _os_log_impl, the
	# function userland os_log() actually calls.  Apple's userland
	# header comes from libtrace and is not published, so the one
	# declaration is appended.
	@if ! grep -q '_os_log_impl' ${SDK_INC}/os/log.h 2>/dev/null; then \
	    cat ${LIBC_EXTRA}/os-log-impl.h >> ${SDK_INC}/os/log.h; \
	fi
	@rm -f ${SDK_INC}/dispatch/CMakeLists.txt
	@rm -rf ${SDK_INC}/dispatch/generic ${SDK_INC}/dispatch/generic_static

	# Kernel-only headers xnu's tree carries and Apple ships none
	# of.  Each is here because it is what actually broke the
	# Darwin module: they reach vm/vm_protos.h and the fixup and
	# kext-collection macros, none of which exist in userland.
.for h in mach/dyld_kernel_fixups.h mach/memory_object_control.h \
	 sys/ubc_internal.h
	@rm -f ${SDK_INC}/${h}
.endfor

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
	# libiconv is its own dylib and not part of libSystem, so nothing
	# else in this SDK stubs it.
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/libiconv.2.dylib \
	    ${SDK_LIB}/libiconv.2.tbd 2>/dev/null || true
	@[ -f ${SDK_LIB}/libiconv.2.tbd ] && \
	    ln -sfn libiconv.2.tbd ${SDK_LIB}/libiconv.tbd || true
	# usr/lib/system.  Apple's SDK carries a stub for each of libSystem's
	# sub-libraries and this one carried none, which is not merely a
	# missing convenience: libSystem.B.tbd names their symbols, but the
	# linker resolves a re-exported symbol through the library that
	# actually defines it, so anything calling into one of these failed
	# to link.  Apple's libarchive calls qtn_file_alloc() and could not
	# be built.
	@mkdir -p ${SDK_LIB}/system
.for l in libcache libcommonCrypto libcompiler_rt libcopyfile \
	  libcorecrypto libcorecrypto_noasm libcorecrypto_trace libdispatch \
	  libdyld libkeymgr libkxld liblaunch \
	  libmacho libmathCommon libmathCommon.A libquarantine \
	  libremovefile libsystem_asl libsystem_blocks libsystem_c \
	  libsystem_collections libsystem_configuration libsystem_containermanager libsystem_coreservices \
	  libsystem_darwin libsystem_darwindirectory libsystem_dnssd libsystem_eligibility \
	  libsystem_featureflags libsystem_info libsystem_kernel libsystem_m \
	  libsystem_malloc libsystem_networkextension libsystem_notify libsystem_platform \
	  libsystem_pthread libsystem_sandbox libsystem_sanitizers libsystem_secinit \
	  libsystem_symptoms libsystem_trace libsystem_trial libunc \
	  libunwind libxpc
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/system/${l}.dylib \
	    ${SDK_LIB}/system/${l}.tbd 2>/dev/null || true
.endfor

	# and the rest, each its own dylib rather than part of libSystem.
.for l v in libz 1 libcurl 4 libedit 3 libexpat 1 libbz2 1.0
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/${l}.${v}.dylib \
	    ${SDK_LIB}/${l}.${v}.tbd 2>/dev/null || true
	@[ -f ${SDK_LIB}/${l}.${v}.tbd ] && \
	    ln -sfn ${l}.${v}.tbd ${SDK_LIB}/${l}.tbd || true
.endfor
	@[ -f ${SDK_LIB}/libobjc.A.tbd ] && \
	    ln -sfn libobjc.A.tbd ${SDK_LIB}/libobjc.tbd || true

# Builds xnu far enough to generate the headers the two blocks above
# take.  Kept out of sdk-headers because it wants the network, a Kernel
# Debug Kit and the better part of an hour; run once, then sdk-headers
# picks the results up on every build after.
xnu-headers:
	@${TOP}/mk/scripts/xnu-headers.sh

# The Swift standard library.
#
# On Darwin the stdlib is not part of the toolchain, it is part of the
# SDK: swiftc resolves Swift.swiftmodule through -sdk, so an SDK
# without one cannot compile a line of Swift.  That is what "unable to
# load standard library" means, and it is not a toolchain fault.
#
# Two things are needed and both come out of the swift port.  The
# .swiftmodule directories carry the interfaces, and shims/ is the
# Clang module the stdlib's own interfaces import as SwiftShims -- with
# the modules alone the stdlib resolves and then fails to typecheck.
#
# The port is behind MK_PORTS, so this installs what is there and says
# what is not.
sdk-swift:
	# The runtime the modules are declared against.  These stub the
	# system's Swift runtime out of the shared cache, exactly as the
	# libSystem and libc++ stubs are made and for the same reason:
	# the dylibs are not on disk to be copied, and an SDK ships stubs
	# rather than libraries anyway.  Without them Swift compiles and
	# then fails to link on _swift_willThrow and its neighbours.
	#
	# Cxx and CxxStdlib have no entry: they have no runtime dylib and
	# Apple's SDK stubs neither.
	@mkdir -p ${SDK_SWIFT}
.for l in libswiftCore libswift_Concurrency libswiftSwiftOnoneSupport \
	  libswift_Builtin_float libswift_StringProcessing
	@${TOP}/mk/scripts/make-tbd.sh /usr/lib/swift/${l}.dylib \
	    ${SDK_SWIFT}/${l}.tbd 2>/dev/null || true
.endfor

.if exists(${SWIFT_LIB}/Swift.swiftmodule)
	@${ECHO} "sdk: installing the Swift standard library"
	@cp -Rf ${SWIFT_LIB}/*.swiftmodule ${SDK_SWIFT}/ 2>/dev/null || true
	@cp -Rf ${SWIFT_SHIMS} ${SDK_SWIFT}/ 2>/dev/null || true
.else
	@${ECHO} "sdk: no swift port built; the SDK will not compile Swift"
	@${ECHO} "sdk:   build it with: bmake MK_PORTS=yes"
.endif

# The Clang module map, which is how Swift reaches the C library.
#
# `import Darwin' names a Clang module, not a Swift one, and this is
# what declares it over the headers installed above.  Without it the
# SDK compiles Swift that touches nothing but the stdlib and no more:
# Synchronization and Observation both stop at "missing required
# module 'Darwin'".
#
# It runs after sdk-headers because the emitter reads the installed
# tree -- every submodule it writes is one whose header is actually
# there -- so running it earlier would describe a smaller SDK than the
# one being built.
	# xnu's EXTERNAL_HEADERS/ vendors copies of headers the compiler
	# itself provides, for the kernel build.  They are Kernel.framework
	# material; Apple's SDK ships none of them in usr/include, and when
	# one lands there it shadows the compiler's real copy -- clang's
	# stdatomic.h found through -isysroot rather than the resource dir
	# yields no memory_order at all.  So take them back out.
.for h in stdarg.h stdatomic.h stdbool.h ptrauth.h ptrcheck.h
	@rm -f ${SDK_INC}/${h}
.endfor
	# Likewise os/log_private.h: earlier builds installed xnu's kernel
	# variant here.  The userspace one is SPI and lives in the internal
	# SDK; the public SDK should carry neither.
	@rm -f ${SDK_INC}/os/log_private.h

sdk-modulemap: sdk-headers
	@${ECHO} "sdk: generating the Darwin module map"
	@CC="${CC}" ${TOP}/mk/scripts/emit-darwin-modulemap.sh ${SDK_INC} \
	    > ${SDK_INC}/Darwin.modulemap
	@{ ${ECHO} "// The SDK's Clang modules.  Apple splits these across a"; \
	   ${ECHO} "// dozen files; ours has the one that matters so far."; \
	   ${ECHO} 'extern module Darwin "Darwin.modulemap"'; \
	   ${ECHO} 'extern module os "Darwin.modulemap"'; \
	   ${ECHO} 'extern module MachO "Darwin.modulemap"'; \
	 } > ${SDK_INC}/module.modulemap

# The Swift overlay for Darwin.
#
# `import Darwin' is a Clang module and a Swift overlay stacked together.
# The module map above declares the first; this builds the second, which
# is where POSIXErrorCode lives and where open, openat and fcntl get the
# non-variadic forms Swift can call.  Without it Swift can name the C
# library and use almost none of it.
#
# Apple ships it prebuilt and stopped building it from source in May
# 2025; lib/swift-darwin-overlay holds the sources from just before that
# commit, and its README says why.  Darwin.swift.gyb is a template, run
# through the swift tree's own gyb first.
#
# A .swiftinterface is emitted beside the binary module, and it is the
# one that matters.  A binary .swiftmodule is tied to the compiler that
# wrote it -- built by the swift port and read by Xcode's swiftc it says
# only "compiled module was created by an older version of the
# compiler" -- while the interface is text any compiler can rebuild
# from.  It is what Apple ships in the SDK for the same reason, and it
# needs library evolution to be emitted at all.
#
# It needs the compiler the swift port builds, so like sdk-swift it does
# what it can and says what it cannot.
sdk-overlay:
.if exists(${SWIFTC}) && exists(${DARWIN_OVERLAY}/Platform.swift)
	@${ECHO} "sdk: building the Darwin Swift overlay"
	@mkdir -p ${OVERLAY_OBJ} ${SDK_SWIFT}/Darwin.swiftmodule
.for g in Darwin tgmath
	@python3 ${GYB} -o ${OVERLAY_OBJ}/${g}.swift \
	    ${DARWIN_OVERLAY}/${g}.swift.gyb
.endfor
.for f in Platform.swift POSIXError.swift MachError.swift
	@cp -f ${DARWIN_OVERLAY}/${f} ${OVERLAY_OBJ}/
.endfor
.for a in arm64 x86_64
	@${SWIFTC} -sdk ${SDK_ROOT} -target ${a}-apple-macosx${XT_DEPLOYMENT_TARGET} \
	    -module-name Darwin -parse-as-library \
	    -enable-library-evolution -swift-version 5 \
	    -emit-module-interface-path \
	    ${SDK_SWIFT}/Darwin.swiftmodule/${a}-apple-macos.swiftinterface \
	    -emit-module -emit-module-path \
	    ${SDK_SWIFT}/Darwin.swiftmodule/${a}-apple-macos.swiftmodule \
	    ${OVERLAY_OBJ}/*.swift 2>/dev/null || \
	    ${ECHO} "sdk: the ${a} Darwin overlay did not build"
	# The binary module goes again once the interface is written.
	# Apple's SDK carries interfaces and docs and no .swiftmodule at
	# all, for the reason above: leaving ours behind means a compiler
	# that is not the one that wrote it picks the binary, refuses it,
	# and never looks at the interface next to it.
	@rm -f ${SDK_SWIFT}/Darwin.swiftmodule/${a}-apple-macos.swiftmodule
.endfor

	# The os overlay, the same way.
	#
	# `import os' is a Clang module and an overlay too, and the
	# overlay is where Logger and the OSLogMessage interpolation live
	# -- the "\(x, privacy: .public)" swift-foundation writes.  The
	# interpolation is Swift's own, from stdlib/private/OSLog; Logger,
	# OSLog and the emitter are ours, because that module stops at a
	# test stub and never had them.  lib/swift-os-overlay says more.
	#
	# It is built with -import-underlying-module: the Swift module and
	# the Clang module share the name os, so it cannot import itself
	# by name.
.if exists(${OS_OVERLAY}/OSLogSupport.swift)
	@${ECHO} "sdk: building the os Swift overlay"
	@mkdir -p ${SDK_SWIFT}/os.swiftmodule
.for a in arm64 x86_64
	@${SWIFTC} -sdk ${SDK_ROOT} -target ${a}-apple-macosx${XT_DEPLOYMENT_TARGET} \
	    -module-name os -parse-as-library -import-underlying-module \
	    -enable-library-evolution -swift-version 5 \
	    -emit-module-interface-path \
	    ${SDK_SWIFT}/os.swiftmodule/${a}-apple-macos.swiftinterface \
	    -emit-module -emit-module-path \
	    ${SDK_SWIFT}/os.swiftmodule/${a}-apple-macos.swiftmodule \
	    ${OS_OVERLAY}/*.swift 2>/dev/null || \
	    ${ECHO} "sdk: the ${a} os overlay did not build"
	@rm -f ${SDK_SWIFT}/os.swiftmodule/${a}-apple-macos.swiftmodule
.endfor
.endif
.else
	@${ECHO} "sdk: no swift port built; the SDK will have no Darwin overlay"
.endif

# The internal SDK.
#
# Apple builds two: the SDK that ships with Xcode, and the one their own
# developers use, which carries the SPI as well.  Ours is for a toolset
# that reimplements Apple's own tools, so it is the second kind it needs
# -- xcodebuild and friends call interfaces the public SDK does not
# declare.
#
# It is a whole SDK and not a delta, which is how Apple's is: everything
# the public one has, and then the private interfaces on top.  So this
# copies the built SDK across first.  usr/ and System/ only -- the two
# SDKSettings files are bundle.mk's, and the internal one names itself
# macosx<version>.internal, which a copy would overwrite.
#
# Where the SPI comes from matters, and the two sources are not equal.
# Most of it this tree already has, from the open-source releases
# themselves -- dyld_priv.h, os/*_private.h, mach/*_private.h, xnu's
# private headers -- and those are the real thing, installed into the
# public SDK already and inherited here with everything else.
#
# lib/apple_internal_sdk is the other kind: headers reconstructed from
# the outside, for interfaces Apple publishes no source for at all --
# corecrypto, sandbox, the private frameworks.  Its own README calls
# them guessed, and neither it nor any other collection of them is
# complete.  They go on last, so nothing derived from real source is
# ever overwritten by a reconstruction, and what is missing stays
# missing rather than being papered over.
sdk-frameworks:
	@${ECHO} "sdk: assembling CoreFoundation.framework"
	# Cleared first: the header install skips what is already there,
	# and CFAvailability.h is appended to below.  Without this an
	# earlier build's copy of that appended block survives, its include
	# guard swallows the new one, and edits to it silently do nothing.
	@rm -rf ${CF_FW}/Versions/A/Headers
	@mkdir -p ${CF_FW}/Versions/A/Headers ${CF_FW}/Versions/A/Modules
	@${TOP}/mk/scripts/install-framework-headers.sh ${CF_SRC} \
	    ${CF_FW}/Versions/A/Headers
	# The annotation macros the drop predates -- see the header.
	@cat ${CF_EXTRA}/cf-nullability.h \
	    >> ${CF_FW}/Versions/A/Headers/CFAvailability.h
	# Apple's own modulemap, less the "requires !exclavekit" on
	# CFPlugInCOM: exclavekit is not a feature this tree's clang knows,
	# and naming an unknown feature makes the module unbuildable rather
	# than conditionally absent.
	@printf 'framework module CoreFoundation [system] {\n\
    umbrella header "CoreFoundation.h"\n\
    explicit module CFPlugInCOM {\n\
        header "CFPlugInCOM.h"\n\
        export *\n\
    }\n\
\n\
    export *\n\
    module * {\n\
        export *\n\
    }\n\
}\n' > ${CF_FW}/Versions/A/Modules/module.modulemap
	# The framework binary itself is the running system's -- this SDK
	# stubs what it links against rather than shipping copies, the same
	# as it does for libSystem.
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation \
	    ${CF_FW}/Versions/A/CoreFoundation.tbd 2>/dev/null || true
	@ln -sfn A ${CF_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${CF_FW}/Headers
	@ln -sfn Versions/Current/Modules ${CF_FW}/Modules
	@ln -sfn Versions/Current/CoreFoundation.tbd ${CF_FW}/CoreFoundation.tbd

	@${ECHO} "sdk: assembling CFNetwork.framework"
	# Apple's CFNetwork drop stops at CFNetwork-129.20, which is from
	# 2005, so this is the 10 public headers that release had.  Apple's
	# SDK ships 12: CFNetworkErrors.h and CFProxySupport.h came later
	# and are in no published source.  The stub is the running system's,
	# so the whole modern API is there to link against -- it is only the
	# declarations that stop in 2005.
	@rm -rf ${CFNET_FW}/Versions/A/Headers
	@mkdir -p ${CFNET_FW}/Versions/A/Headers ${CFNET_FW}/Versions/A/Modules
	@${TOP}/mk/scripts/install-framework-headers.sh ${CFNET_SRC} \
	    ${CFNET_FW}/Versions/A/Headers CFNetwork
	# Two fixups, both consequences of the drop's age.
	#
	# CFHost.h and CFNetServices.h declare their callbacks with
	# CALLBACK_API_C, which is ConditionalMacros.h's, and nothing in
	# the drop includes that header -- in 2005 it arrived through
	# Carbon.  Both include CFNetworkDefs.h first, so it goes there.
	#
	@printf '\n#include <ConditionalMacros.h>\n' \
	    >> ${CFNET_FW}/Versions/A/Headers/CFNetworkDefs.h
	# The same two wrap their structures in "#pragma options
	# align=mac68k", which clang rejects outright on arm64.  It is
	# translated to #pragma pack rather than dropped: measured against
	# Apple's own SDK, CFHostClientContext is 40 bytes either way, but
	# Apple's carries an alignment of 2 and the unpacked version has 8.
	# The members are all pointer-sized so no offset moves, but the
	# type's alignment is part of its ABI, and matching it is the point
	# of this tree.  align=reset becomes the matching pop.
.for h in CFHost.h CFNetServices.h
	@LC_ALL=C sed -i '' \
	    -e 's|^#pragma options align=mac68k|#pragma pack(push, 2)|' \
	    -e 's|^#pragma options align=reset|#pragma pack(pop)|' \
	    ${CFNET_FW}/Versions/A/Headers/${h} 2>/dev/null || true
.endfor
	@printf 'framework module CFNetwork [system] {\n\
    umbrella header "CFNetwork.h"\n\
    export *\n\
    module * {\n\
        export *\n\
    }\n\
}\n' > ${CFNET_FW}/Versions/A/Modules/module.modulemap
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/CFNetwork.framework/Versions/A/CFNetwork \
	    ${CFNET_FW}/Versions/A/CFNetwork.tbd 2>/dev/null || true
	@ln -sfn A ${CFNET_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${CFNET_FW}/Headers
	@ln -sfn Versions/Current/Modules ${CFNET_FW}/Modules
	@ln -sfn Versions/Current/CFNetwork.tbd ${CFNET_FW}/CFNetwork.tbd

	@${ECHO} "sdk: assembling CoreServices.framework"
	@mkdir -p ${CS_FW}/Versions/A/Headers ${CS_FW}/Versions/A/Modules \
	    ${FSE_FW}/Versions/A/Headers ${FSE_FW}/Versions/A/Modules
	@cp -f ${CORESERVICES}/CoreServices/CoreServices.h \
	    ${CS_FW}/Versions/A/Headers/
	@cp -f ${CORESERVICES}/FSEvents/FSEvents.h \
	    ${FSE_FW}/Versions/A/Headers/
	@printf 'framework module CoreServices [system] {\n\
    umbrella header "CoreServices.h"\n\
    export *\n\
    module * {\n\
        export *\n\
    }\n\
}\n' > ${CS_FW}/Versions/A/Modules/module.modulemap
	@printf 'framework module FSEvents [system] {\n\
    umbrella header "FSEvents.h"\n\
    export *\n\
    module * {\n\
        export *\n\
    }\n\
}\n' > ${FSE_FW}/Versions/A/Modules/module.modulemap
	# FSEvents ships inside CoreServices, so both get a stub: the
	# umbrella re-exports what the subframework defines, and a linker
	# given -framework CoreServices has to resolve the FSEvents
	# symbols through it.
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
	    ${CS_FW}/Versions/A/CoreServices.tbd 2>/dev/null || true
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/FSEvents.framework/Versions/A/FSEvents \
	    ${FSE_FW}/Versions/A/FSEvents.tbd 2>/dev/null || true
	@ln -sfn A ${CS_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${CS_FW}/Headers
	@ln -sfn Versions/Current/Modules ${CS_FW}/Modules
	@ln -sfn Versions/Current/Frameworks ${CS_FW}/Frameworks
	@ln -sfn Versions/Current/CoreServices.tbd ${CS_FW}/CoreServices.tbd
	@ln -sfn A ${FSE_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${FSE_FW}/Headers
	@ln -sfn Versions/Current/Modules ${FSE_FW}/Modules
	@ln -sfn Versions/Current/FSEvents.tbd ${FSE_FW}/FSEvents.tbd

	@${ECHO} "sdk: assembling Security.framework"
	# Security's own build stages its public headers into
	# header_symlinks/Security, which is the set the framework
	# installs -- so take it rather than trying to work out which of
	# the source tree's headers are public.
	@mkdir -p ${SEC_FW}/Versions/A/Headers ${SEC_FW}/Versions/A/Modules
	@cp -Lf ${SECURITY}/header_symlinks/Security/*.h \
	    ${SEC_FW}/Versions/A/Headers/ 2>/dev/null || true
	# macOS/ carries the platform-only half of the same public set --
	# the CSSM headers above all -- and SecCertificate.h includes
	# Security/cssmtype.h straight out of it.
	@cp -Lf ${SECURITY}/header_symlinks/macOS/Security/*.h \
	    ${SEC_FW}/Versions/A/Headers/ 2>/dev/null || true
	@printf 'framework module Security [system] {\n\
    umbrella header "Security.h"\n\
    export *\n\
    module * {\n\
        export *\n\
    }\n\
}\n' > ${SEC_FW}/Versions/A/Modules/module.modulemap
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/Security.framework/Versions/A/Security \
	    ${SEC_FW}/Versions/A/Security.tbd 2>/dev/null || true
	@ln -sfn A ${SEC_FW}/Versions/Current
	@ln -sfn Versions/Current/Headers ${SEC_FW}/Headers
	@ln -sfn Versions/Current/Modules ${SEC_FW}/Modules
	@ln -sfn Versions/Current/Security.tbd ${SEC_FW}/Security.tbd

	@${ECHO} "sdk: stubbing Foundation.framework"
	# Stub only, no headers.  Foundation's headers are Objective-C and
	# Apple publishes none of them; reimplementing them is its own
	# project.  What is needed here is narrower: Apple's git links
	# git-credential-osxkeychain against Foundation without including
	# any of it, so the framework has to be linkable and nothing more.
	# A .tbd of the running system's Foundation is exactly that, and it
	# is the same thing this SDK does for libSystem.
	@mkdir -p ${FOUNDATION_FW}/Versions/A
	@${TOP}/mk/scripts/make-tbd.sh \
	    /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation \
	    ${FOUNDATION_FW}/Versions/A/Foundation.tbd 2>/dev/null || true
	@ln -sfn A ${FOUNDATION_FW}/Versions/Current
	@ln -sfn Versions/Current/Foundation.tbd ${FOUNDATION_FW}/Foundation.tbd

sdk-internal: sdk-headers sdk-modulemap sdk-stubs sdk-swift sdk-overlay sdk-frameworks
	@${ECHO} "sdk: assembling MacOSX.Internal.sdk"
	@mkdir -p ${INTERNAL_SDK}/usr/include ${INTERNAL_SDK}/usr/lib \
	    ${INTERNAL_SDK}/usr/local/include ${INTERNAL_SDK}/usr/local/lib \
	    ${INTERNAL_SDK}/System/Library/Frameworks \
	    ${INTERNAL_SDK}/System/Library/PrivateFrameworks
.for d in usr System
	@cp -Rf ${SDK_ROOT}/${d}/. ${INTERNAL_SDK}/${d}/ 2>/dev/null || true
.endfor
	# xnu installs its SPI under usr/local/include, and Apple's public
	# SDK ships none of it -- that directory is empty in Xcode's
	# MacOSX.sdk.  So the whole tree goes here and only here: firehose,
	# machine/cpu_capabilities.h, the private mach and sys headers, the
	# lot.  This is what makes the internal SDK unrestricted.
	# The rest of syslog's headers, which are private.
	@cp -f ${SYSLOG}/libsystem_asl.tproj/include/*.h \
	    ${INTERNAL_SDK}/usr/local/include/ 2>/dev/null || true
	@rm -f ${INTERNAL_SDK}/usr/local/include/asl.h
	# usr/include/System, which is how Apple's own projects reach the
	# System.framework private headers -- libarchive says
	# #include <System/sys/fsctl.h>.  The fakeroot's PrivateHeaders is
	# that set; only a partial copy of it was here before.
.if exists(${XNU_FAKEROOT}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders)
	@mkdir -p ${INTERNAL_SDK}/usr/include/System
	@cp -Rf ${XNU_FAKEROOT}/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/. \
	    ${INTERNAL_SDK}/usr/include/System/ 2>/dev/null || true
.endif

	# libpthread's private headers.  The eight public ones are already
	# in usr/include/pthread; these ten are the SPI beside them, and
	# Apple's libarchive includes <pthread/private.h>.
	@mkdir -p ${INTERNAL_SDK}/usr/local/include/pthread
	@cp -f ${LIBPTHREAD}/private/pthread/*.h \
	    ${INTERNAL_SDK}/usr/local/include/pthread/ 2>/dev/null || true

	# Reconstructed SPI from lib/apple-spi.  Internal SDK only: these
	# are interfaces no SDK publishes, so they follow the same rule as
	# xnu's usr/local/include.
	@mkdir -p ${INTERNAL_SDK}/usr/local/include
	@cp -f ${APPLE_SPI}/*.h ${INTERNAL_SDK}/usr/local/include/ 2>/dev/null || true
	# rootless.h at the top level as well as under sandbox/.  Apple's
	# perl says #include <rootless.h> plainly, which is evidence for
	# where their internal SDK keeps it; usr/local/include is where
	# this tree puts SPI, and it is on the include path after
	# usr/include.  The symbols are in libSystem either way.
.if exists(${INTERNAL_SPI}/usr/include/sandbox/rootless.h)
	@mkdir -p ${INTERNAL_SDK}/usr/local/include
	@cp -f ${INTERNAL_SPI}/usr/include/sandbox/rootless.h \
	    ${INTERNAL_SDK}/usr/local/include/ 2>/dev/null || true
.endif
	# The userspace os/log_private.h, reconstructed -- see the header.
	# SPI, so it goes here and not in the public SDK.
	@mkdir -p ${INTERNAL_SDK}/usr/include/os
	@cp -f ${LIBC_EXTRA}/os/log_private.h \
	    ${INTERNAL_SDK}/usr/include/os/ 2>/dev/null || true
	# Apple's public SDK carries Kernel.framework with Headers only --
	# PrivateHeaders ships in the KDK.  The internal SDK is where they
	# belong here.
.if exists(${XNU_FAKEROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders)
	@${ECHO} "sdk: adding Kernel.framework PrivateHeaders"
	@mkdir -p ${INTERNAL_KERNEL_FW}/Versions/A/PrivateHeaders
	@cp -Rf ${XNU_FAKEROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/. \
	    ${INTERNAL_KERNEL_FW}/Versions/A/PrivateHeaders/ 2>/dev/null || true
.endif
.if exists(${XNU_FAKEROOT}/usr/local/include)
	@${ECHO} "sdk: adding xnu's SPI (usr/local/include)"
	@mkdir -p ${INTERNAL_SDK}/usr/local/include
	@cp -Rf ${XNU_FAKEROOT}/usr/local/include/. \
	    ${INTERNAL_SDK}/usr/local/include/ 2>/dev/null || true
.endif
.if exists(${INTERNAL_SPI}/usr/include)
	@${ECHO} "sdk: adding the reconstructed SPI headers"
.for d in usr System
	@cp -Rf ${INTERNAL_SPI}/${d}/. ${INTERNAL_SDK}/${d}/ 2>/dev/null || true
.endfor
.else
	@${ECHO} "sdk: no lib/apple_internal_sdk; the internal SDK is the public one"
.endif
	# this SDK is assembled with cp -Rf, which merges and never deletes,
	# so a header pruned upstream lives on here from an earlier build.
.for h in stdarg.h stdatomic.h stdbool.h ptrauth.h ptrcheck.h
	@rm -f ${INTERNAL_SDK}/usr/include/${h}
.endfor
	# An earlier version of this file wrote Kernel.framework's
	# PrivateHeaders to ${INTERNAL_SDK}/${KERNEL_FW}, and KERNEL_FW is
	# an absolute path -- so the whole of it landed under a Users
	# directory at the root of this SDK.  The bug is gone; the tree it
	# made is not, because nothing here ever deletes.
	@rm -rf ${INTERNAL_SDK}/Users

sdk: sdk-headers sdk-modulemap sdk-stubs sdk-swift sdk-overlay sdk-frameworks \
	sdk-internal

.PHONY: sdk sdk-headers sdk-modulemap sdk-stubs sdk-swift sdk-overlay \
	sdk-frameworks sdk-internal xnu-headers
