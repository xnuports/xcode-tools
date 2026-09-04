# mk/bundle.mk
#
# Emits the bundle metadata that makes build/release/ a drop-in
# replacement for Xcode's Developer directory:
#
#	Toolchains/XcodeDefault.xctoolchain/ToolchainInfo.plist
#	Platforms/MacOSX.platform/Info.plist
#	Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/SDKSettings.plist
#
# The programs themselves are installed by mk/progs.mk entries; this
# file supplies only the metadata and the toolchain shim scripts, and
# creates the directory skeleton those bundles need.
#
# Note the SDK is a skeleton: it carries settings but no headers or
# libraries yet.  Populating usr/include and System/ is a later stage
# (docs/DOCUMENTATION.md section 8).  The skeleton is emitted anyway
# because it defines the layout every later stage installs into.

TOP?=		${.CURDIR}

.include "${TOP}/mk/xcodetools.sys.mk"

# A platform description, when one is named.  It is included before the
# defaults below so its plain assignments stand: everything macOS-shaped
# here is a ?= for exactly that reason.  See mk/platform.d.
.if defined(XT_PLATFORM_MK)
.include "${XT_PLATFORM_MK}"
.endif

RELEASE=	${TOP}/build/release
CONFIGS=	${TOP}/configs
SCRIPTS=	${TOP}/scripts

# Names follow Apple's, because that is what makes the tree a drop-in:
# consumers look up "XcodeDefault.xctoolchain" and "MacOSX.sdk" by name.
XT_PLATFORM?=	MacOSX
XT_SDK?=	MacOSX
XT_TOOLCHAIN?=	XcodeDefault

# The internal SDK, which Apple builds the system against and does not
# ship.  It is the same macOS SDK plus the headers and libraries kept
# out of the public one, so it is emitted beside it under the same
# platform and answers to the canonical name with ".internal" on the
# end -- the name xcodebuild and xcrun already look for.  Only the
# layout is produced here; filling it in is separate work.
XT_SDK_INTERNAL?=	MacOSX.Internal

PLATFORM_DIR=	${RELEASE}/Platforms/${XT_PLATFORM}.platform
SDK_DIR=	${PLATFORM_DIR}/Developer/SDKs/${XT_SDK}.sdk
INTERNAL_SDK_DIR=	${PLATFORM_DIR}/Developer/SDKs/${XT_SDK_INTERNAL}.sdk

# What distinguishes an internal SDK on disk.  usr/local is the whole
# point of it: the public SDK has no usr/local at all, and that is where
# the headers and libraries Apple keeps to itself live.  The rest mirrors
# the public bundle so the two are interchangeable to anything that
# walks an SDK.  PrivateFrameworks is listed because the public SDK does
# carry one -- the stubs are public, the headers behind them are not.
INTERNAL_SDK_SUBDIRS=	usr/include \
			usr/lib \
			usr/local/include \
			usr/local/lib \
			System/Library/Frameworks \
			System/Library/PrivateFrameworks
TC_DIR=		${RELEASE}/${XCTOOLCHAIN}

# Version and deployment target are taken from the host SDK, so the
# emitted bundle is coherent with what the tools would actually build
# against.  Override on the command line to pin them.
# != is unconditional, so it has to be guarded: a platform description
# sets its own version from its own SDK, and DriverKit's numbering is a
# release behind the rest.
.if !defined(XT_SDK_VERSION)
XT_SDK_VERSION!=	xcrun --show-sdk-version 2>/dev/null || echo 0.0
.endif
XT_DEPLOYMENT_TARGET?=	${XT_SDK_VERSION}
XT_SDK_CANONICAL?=	macosx${XT_SDK_VERSION}
XT_SDK_INTERNAL_CANONICAL?=	macosx${XT_SDK_VERSION}.internal
XT_DEFAULT_ARCH!=	uname -m 2>/dev/null || echo arm64

# The architectures the SDK can build for, which is not the same as the
# one it is being built on: the stubs this tree generates carry
# arm64, arm64e and x86_64, and a build asks the SDK what it supports
# before it will compile for anything.  arm64e is left out for the same
# reason Apple leaves it out -- it is not an architecture a project
# asks for by name.
XT_SDK_ARCHS?=		arm64 x86_64

# The toolchain identifier is Apple's on purpose: it is the name build
# systems look up to find the default toolchain, so a replacement has to
# answer to it, exactly as our tools have to answer to Apple's argv.
XT_TOOLCHAIN_ID?=	com.apple.dt.toolchain.XcodeDefault
XT_PLATFORM_ID?=	com.apple.platform.macosx

# The rest of a platform's identity.  These defaulted to macOS's when
# this file emitted only that one; they are named now because the same
# recipe emits iPhoneOS, AppleTVOS, WatchOS and DriverKit, each with
# mk/platform.d/<name>.mk supplying its own.
XT_FAMILY_ID?=		macosx
XT_FAMILY_NAME?=	macOS
XT_PLATFORM_NAME?=	macOS Platform
XT_DESCRIPTION?=	macOS

# The platforms beside macOS.  Each is the same recipe with a different
# description: bundle-dirs lays the directories out, bundle-platform
# writes Info.plist and bundle-sdk the SDKSettings, so what comes out is
# a bundle of the right shape with an empty SDK inside it -- which is
# how MacOSX.platform started too.
#
# The toolchain is not emitted per platform.  There is one toolchain and
# every platform builds with it, which is Apple's arrangement as well.
XT_OTHER_PLATFORMS?=	iPhoneOS AppleTVOS WatchOS DriverKit

platforms:
.for p in ${XT_OTHER_PLATFORMS}
	@${MAKE} -f ${TOP}/mk/bundle.mk TOP=${TOP} \
	    XT_PLATFORM_MK=${TOP}/mk/platform.d/${p}.mk \
	    bundle-dirs bundle-platform bundle-sdk > /dev/null
	@${ECHO} "   platform:  ${p}.platform"
.endfor

# Finder leaves these behind in a tree anyone browses, and they are not
# build output.  One of them once kept a directory alive through a move
# that should have emptied it.
bundle-clean-strays:
	@find ${RELEASE} -name '.DS_Store' -delete 2>/dev/null || true

bundles: bundle-dirs bundle-toolchain bundle-platform bundle-sdk bundle-shims bundle-config bundle-aliases bundle-makefiles platforms bundle-clean-strays
	@${ECHO} "== bundles emitted =="
	@${ECHO} "   toolchain: ${XCTOOLCHAIN} (${XT_TOOLCHAIN_ID})"
	@${ECHO} "   sdk:       ${XT_SDK}.sdk ${XT_SDK_VERSION} (${XT_SDK_CANONICAL})"
	@${ECHO} "   sdk:       ${XT_SDK_INTERNAL}.sdk ${XT_SDK_VERSION} (${XT_SDK_INTERNAL_CANONICAL}), layout only"

bundle-dirs:
.for d in ${XCTOOLCHAIN}/usr/bin ${XCTOOLCHAIN}/usr/lib ${XCTOOLCHAIN}/usr/libexec \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/usr/include \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/usr/lib \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/System/Library/Frameworks \
	  ${INTERNAL_SDK_SUBDIRS:S|^|Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK_INTERNAL}.sdk/|} \
	  Platforms/${XT_PLATFORM}.platform/Developer/Library
	@mkdir -p ${RELEASE}/${d}
.endfor

# --- xcrun configuration ---------------------------------------------
#
# xcrun reads its default SDK/toolchain selection from
# <developer_dir>/usr/share/xcrun.ini, falling back to the absolute
# XCRUN_DEFAULT_CFG compiled in for an installed system copy.  Emitting
# it here keeps the release tree self-contained and relocatable, and
# keeps the names in step with the bundles actually emitted above.

bundle-config: ${RELEASE}/usr/share/xcrun.ini

${RELEASE}/usr/share/xcrun.ini: ${CONFIGS}/xcrun.ini
	@mkdir -p ${.TARGET:H}
	@sed -e 's|^name = .*|@@|' ${CONFIGS}/xcrun.ini \
	  | awk '/^\[SDK\]/{print;insdk=1;next} \
		 /^\[TOOLCHAIN\]/{print;insdk=0;next} \
		 /^@@$$/{ if (insdk) print "name = ${XT_SDK}"; \
			  else print "name = ${XT_TOOLCHAIN}"; next } \
		 {print}' > ${.TARGET}

# --- toolchain --------------------------------------------------------

bundle-toolchain: ${TC_DIR}/ToolchainInfo.plist

${TC_DIR}/ToolchainInfo.plist:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '<?xml version="1.0" encoding="UTF-8"?>'; \
	  echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'; \
	  echo '<plist version="1.0">'; \
	  echo '<dict>'; \
	  echo '	<key>Identifier</key>'; \
	  echo '	<string>${XT_TOOLCHAIN_ID}</string>'; \
	  echo '</dict>'; \
	  echo '</plist>'; \
	} > ${.TARGET}

# --- platform ---------------------------------------------------------

bundle-platform: ${PLATFORM_DIR}/Info.plist ${PLATFORM_DIR}/version.plist

# Apple's platform bundles carry one of these beside Info.plist, and
# anything reading a platform's version looks here rather than at the
# SDK's.  ProductBuildVersion is the host's build, which is what their
# PlatformSupport records too.
XT_BUILD_VERSION!=	sw_vers -buildVersion 2>/dev/null || echo 0

${PLATFORM_DIR}/version.plist:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '<?xml version="1.0" encoding="UTF-8"?>'; \
	  echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'; \
	  echo '<plist version="1.0">'; \
	  echo '<dict>'; \
	  echo '	<key>CFBundleShortVersionString</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>CFBundleVersion</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>ProductBuildVersion</key>'; \
	  echo '	<string>${XT_BUILD_VERSION}</string>'; \
	  echo '	<key>ProjectName</key>'; \
	  echo '	<string>PlatformSupport</string>'; \
	  echo '</dict>'; \
	  echo '</plist>'; \
	} > ${.TARGET}

${PLATFORM_DIR}/Info.plist:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '<?xml version="1.0" encoding="UTF-8"?>'; \
	  echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'; \
	  echo '<plist version="1.0">'; \
	  echo '<dict>'; \
	  echo '	<key>CFBundleIdentifier</key>'; \
	  echo '	<string>${XT_PLATFORM_ID}</string>'; \
	  echo '	<key>CFBundleName</key>'; \
	  echo '	<string>${XT_PLATFORM_NAME}</string>'; \
	  echo '	<key>CFBundleShortVersionString</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>CFBundleVersion</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>Version</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>Description</key>'; \
	  echo '	<string>${XT_DESCRIPTION}</string>'; \
	  echo '	<key>FamilyIdentifier</key>'; \
	  echo '	<string>${XT_FAMILY_ID}</string>'; \
	  echo '	<key>FamilyName</key>'; \
	  echo '	<string>${XT_FAMILY_NAME}</string>'; \
	  echo '	<key>Identifier</key>'; \
	  echo '	<string>${XT_PLATFORM_ID}</string>'; \
	  echo '	<key>Type</key>'; \
	  echo '	<string>Platform</string>'; \
	  echo '</dict>'; \
	  echo '</plist>'; \
	} > ${.TARGET}

# --- sdk --------------------------------------------------------------

bundle-sdk: ${SDK_DIR}/SDKSettings.plist ${INTERNAL_SDK_DIR}/SDKSettings.plist \
	${SDK_DIR}/SDKSettings.json ${INTERNAL_SDK_DIR}/SDKSettings.json

# Both bundles come out of one emitter so their contents cannot drift
# apart; what differs is the identity passed to it.
SDKSETTINGS=	${TOP}/mk/scripts/emit-sdksettings.sh

${SDK_DIR}/SDKSettings.plist: ${SDKSETTINGS}
	@mkdir -p ${.TARGET:H}
	@${SDKSETTINGS} ${XT_SDK_CANONICAL} "${XT_DESCRIPTION} ${XT_SDK_VERSION}" \
	    ${XT_SDK_VERSION} ${XT_DEPLOYMENT_TARGET} "${XT_SDK_ARCHS}" \
	    > ${.TARGET}

${INTERNAL_SDK_DIR}/SDKSettings.plist: ${SDKSETTINGS}
	@mkdir -p ${.TARGET:H}
	@${SDKSETTINGS} ${XT_SDK_INTERNAL_CANONICAL} \
	    "${XT_DESCRIPTION} ${XT_SDK_VERSION} Internal" \
	    ${XT_SDK_VERSION} ${XT_DEPLOYMENT_TARGET} "${XT_SDK_ARCHS}" \
	    > ${.TARGET}

# Apple ships the same settings twice, as a plist and as JSON, and
# different readers reach for different ones: swiftc looks only for
# SDKSettings.json and warns on every invocation when it is absent.
# Converting rather than emitting it a second time keeps the two from
# saying different things.
.for d in ${SDK_DIR} ${INTERNAL_SDK_DIR}
${d}/SDKSettings.json: ${d}/SDKSettings.plist
	@plutil -convert json -o ${.TARGET} ${.ALLSRC}
.endfor

# --- toolchain shims --------------------------------------------------
#
# --- Makefiles/ -------------------------------------------------------
#
# Xcode ships a Makefiles directory holding the build-system fragments
# other projects include: Carbon, CoreOS, VersioningSystems and
# pb_makefiles.  Two of those we have sources for.
#
# CoreOSMakefiles' own install rules do three things -- copy the tree,
# drop the copied Makefile, and generate Standard/{Commands,Variables}.make
# from the .in templates with unifdef -- and that is reproduced here.
# The unifdef used is the one this tree builds, from developer_cmds.
#
# bsdmake's system makefiles go alongside, since it reports "no system
# rules (sys.mk)" without them and macOS has no /usr/share/mk, which is
# the path compiled into it.  bsdmake takes them with -m:
#
#	bsdmake -m <developer>/usr/local/share/bsdmake/mk ...
#
# (bmake uses MAKESYSPATH for the same purpose; the two differ.)

DEVTOOLS=	${TOP}/src/apple/distribution-Developer_Tools
UNIFDEF=	${TC_DIR}/usr/bin/unifdef

bundle-makefiles: bundle-dirs
	@mkdir -p ${RELEASE}/Makefiles
.if exists(${DEVTOOLS}/CoreOSMakefiles)
	@rsync -a --delete --exclude '.git' \
		${DEVTOOLS}/CoreOSMakefiles/ ${RELEASE}/Makefiles/CoreOS/
	@rm -f ${RELEASE}/Makefiles/CoreOS/Makefile
	@if [ -x ${UNIFDEF} ]; then \
		for i in Commands Variables; do \
			[ -f ${RELEASE}/Makefiles/CoreOS/Standard/$$i.in ] || continue; \
			${UNIFDEF} -UBSDMAKESTYLE -t \
				${RELEASE}/Makefiles/CoreOS/Standard/$$i.in \
				> ${RELEASE}/Makefiles/CoreOS/Standard/$$i.make; \
			rm -f ${RELEASE}/Makefiles/CoreOS/Standard/$$i.in; \
		done; \
		${ECHO} "makefiles: CoreOS (Commands/Variables generated with our unifdef)"; \
	 else \
		${ECHO} "makefiles: CoreOS (unifdef not built; .in templates left in place)"; \
	 fi
.endif
.if exists(${DEVTOOLS}/pb_makefiles)
	@rsync -a --delete --exclude '.git' \
		${DEVTOOLS}/pb_makefiles/ ${RELEASE}/Makefiles/pb_makefiles/
	@${ECHO} "makefiles: pb_makefiles"
.endif
.if exists(${TOP}/src/extras/bsdmake/mk)
	@mkdir -p ${RELEASE}/usr/local/share/bsdmake
	@rsync -a --delete --exclude '.git' \
		${TOP}/src/extras/bsdmake/mk/ ${RELEASE}/usr/local/share/bsdmake/mk/
	@${ECHO} "makefiles: bsdmake system rules"
.endif

# --- Apple's tool aliases ---------------------------------------------
#
# A stock toolchain points nm and otool at the LLVM implementations,
# keeping the cctools builds beside them as nm-classic and otool-classic
# (which is how mk/progs.mk installs ours).  Recreate those two links
# once the llvm port has supplied the targets; without it the names are
# simply absent rather than wrong.

# Alias names, as <alias> <target> pairs.  ld.lld is how a driver asks
# for the ELF linker (clang's -fuse-ld=lld looks for exactly that name),
# and the llvm-* aliases are the ones those tools answer to when they are
# used as their binutils counterparts.  `ld` is not among them: that is
# ld64, the Mach-O linker, and it stays.
bundle-aliases: bundle-dirs
.for a t in nm llvm-nm otool llvm-otool ld.lld lld \
	    swift swift-frontend swiftc swift-frontend \
	    llvm-readelf llvm-readobj llvm-strip llvm-objcopy \
	    llvm-ranlib llvm-ar
	@if [ -e ${TC_DIR}/usr/bin/${t} ] && [ ! -e ${TC_DIR}/usr/bin/${a} ]; then \
		ln -sfn ${t} ${TC_DIR}/usr/bin/${a}; \
		${ECHO} "alias: ${a} -> ${t}"; \
	 fi
.endfor

# --- toolchain shims --------------------------------------------------
#
# A stock toolchain ships cc and c++ as symlinks to clang and cpp as a
# small script, so that is what we emit.  The cc/c++/clang wrapper
# scripts this project carried before existed only because there was no
# real clang to point at; the llvm port supplies one now, and a wrapper
# that resolves its tool with "xcrun -find $0" would find itself.
#
# Nothing is overwritten: a name a port already provided is left alone.

SHIM_BIN=	${TC_DIR}/usr/bin

bundle-shims: bundle-dirs
	@mkdir -p ${SHIM_BIN}
.for s in cc c++
	@if [ -e ${SHIM_BIN}/${s} ]; then \
		${ECHO} "shim: ${s} already provided by a port, not overwriting"; \
	 elif [ -e ${SHIM_BIN}/clang ]; then \
		ln -sfn clang ${SHIM_BIN}/${s}; \
	 fi
.endfor
	@[ -e ${SHIM_BIN}/clang++ ] || [ ! -e ${SHIM_BIN}/clang ] || \
		ln -sfn clang ${SHIM_BIN}/clang++
	@if [ ! -e ${SHIM_BIN}/cpp ]; then \
		cp ${SCRIPTS}/cpp.sh ${SHIM_BIN}/cpp && chmod 755 ${SHIM_BIN}/cpp; \
	 fi
	@mkdir -p ${RELEASE}/usr/bin
	@cp ${SCRIPTS}/xcrun-tool.sh ${RELEASE}/usr/bin/xcrun-tool
	@chmod 755 ${RELEASE}/usr/bin/xcrun-tool

.PHONY: bundles bundle-dirs bundle-toolchain bundle-platform bundle-sdk \
	bundle-shims bundle-config bundle-aliases bundle-makefiles
