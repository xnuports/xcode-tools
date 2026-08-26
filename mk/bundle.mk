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

RELEASE=	${TOP}/build/release
CONFIGS=	${TOP}/configs
SCRIPTS=	${TOP}/scripts

# Names follow Apple's, because that is what makes the tree a drop-in:
# consumers look up "XcodeDefault.xctoolchain" and "MacOSX.sdk" by name.
XT_PLATFORM?=	MacOSX
XT_SDK?=	MacOSX
XT_TOOLCHAIN?=	XcodeDefault

PLATFORM_DIR=	${RELEASE}/Platforms/${XT_PLATFORM}.platform
SDK_DIR=	${PLATFORM_DIR}/Developer/SDKs/${XT_SDK}.sdk
TC_DIR=		${RELEASE}/${XCTOOLCHAIN}

# Version and deployment target are taken from the host SDK, so the
# emitted bundle is coherent with what the tools would actually build
# against.  Override on the command line to pin them.
XT_SDK_VERSION!=	xcrun --show-sdk-version 2>/dev/null || echo 0.0
XT_DEPLOYMENT_TARGET?=	${XT_SDK_VERSION}
XT_SDK_CANONICAL?=	macosx${XT_SDK_VERSION}
XT_DEFAULT_ARCH!=	uname -m 2>/dev/null || echo arm64

# The toolchain identifier is Apple's on purpose: it is the name build
# systems look up to find the default toolchain, so a replacement has to
# answer to it, exactly as our tools have to answer to Apple's argv.
XT_TOOLCHAIN_ID?=	com.apple.dt.toolchain.XcodeDefault
XT_PLATFORM_ID?=	com.apple.platform.macosx

bundles: bundle-dirs bundle-toolchain bundle-platform bundle-sdk bundle-compat bundle-shims bundle-config bundle-aliases
	@${ECHO} "== bundles emitted =="
	@${ECHO} "   toolchain: ${XCTOOLCHAIN} (${XT_TOOLCHAIN_ID})"
	@${ECHO} "   sdk:       ${XT_SDK}.sdk ${XT_SDK_VERSION} (${XT_SDK_CANONICAL})"

bundle-dirs:
.for d in ${XCTOOLCHAIN}/usr/bin ${XCTOOLCHAIN}/usr/lib ${XCTOOLCHAIN}/usr/libexec \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/usr/include \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/usr/lib \
	  Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk/System/Library/Frameworks \
	  Platforms/${XT_PLATFORM}.platform/Developer/Library \
	  SDKs Toolchains
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

bundle-toolchain: ${TC_DIR}/ToolchainInfo.plist ${TC_DIR}/info.ini

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

# info.ini is our own xcrun's metadata format.  It sits inside the
# bundle so that xcrun keeps resolving while it still speaks the older
# layout; teaching xcrun to read ToolchainInfo.plist is stage 4.
${TC_DIR}/info.ini:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '[TOOLCHAIN]'; \
	  echo 'name = ${XT_TOOLCHAIN}'; \
	  echo 'version = ${XT_SDK_VERSION}'; \
	} > ${.TARGET}

# --- platform ---------------------------------------------------------

bundle-platform: ${PLATFORM_DIR}/Info.plist

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
	  echo '	<string>macOS Platform</string>'; \
	  echo '	<key>CFBundleShortVersionString</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>CFBundleVersion</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>Description</key>'; \
	  echo '	<string>macOS</string>'; \
	  echo '	<key>FamilyIdentifier</key>'; \
	  echo '	<string>macosx</string>'; \
	  echo '	<key>FamilyName</key>'; \
	  echo '	<string>macOS</string>'; \
	  echo '	<key>Identifier</key>'; \
	  echo '	<string>${XT_PLATFORM_ID}</string>'; \
	  echo '	<key>Type</key>'; \
	  echo '	<string>Platform</string>'; \
	  echo '</dict>'; \
	  echo '</plist>'; \
	} > ${.TARGET}

# --- sdk --------------------------------------------------------------

bundle-sdk: ${SDK_DIR}/SDKSettings.plist ${SDK_DIR}/info.ini

${SDK_DIR}/SDKSettings.plist:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '<?xml version="1.0" encoding="UTF-8"?>'; \
	  echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'; \
	  echo '<plist version="1.0">'; \
	  echo '<dict>'; \
	  echo '	<key>CanonicalName</key>'; \
	  echo '	<string>${XT_SDK_CANONICAL}</string>'; \
	  echo '	<key>DisplayName</key>'; \
	  echo '	<string>macOS ${XT_SDK_VERSION}</string>'; \
	  echo '	<key>MinimalDisplayName</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>Version</key>'; \
	  echo '	<string>${XT_SDK_VERSION}</string>'; \
	  echo '	<key>DefaultDeploymentTarget</key>'; \
	  echo '	<string>${XT_DEPLOYMENT_TARGET}</string>'; \
	  echo '	<key>DefaultProperties</key>'; \
	  echo '	<dict>'; \
	  echo '		<key>PLATFORM_NAME</key>'; \
	  echo '		<string>macosx</string>'; \
	  echo '		<key>DEFAULT_COMPILER</key>'; \
	  echo '		<string>com.apple.compilers.llvm.clang.1_0</string>'; \
	  echo '	</dict>'; \
	  echo '	<key>SupportedTargets</key>'; \
	  echo '	<dict>'; \
	  echo '		<key>macosx</key>'; \
	  echo '		<dict>'; \
	  echo '			<key>Archs</key>'; \
	  echo '			<array>'; \
	  echo '				<string>${XT_DEFAULT_ARCH}</string>'; \
	  echo '			</array>'; \
	  echo '			<key>DefaultDeploymentTarget</key>'; \
	  echo '			<string>${XT_DEPLOYMENT_TARGET}</string>'; \
	  echo '			<key>PlatformFamilyName</key>'; \
	  echo '			<string>macOS</string>'; \
	  echo '		</dict>'; \
	  echo '	</dict>'; \
	  echo '</dict>'; \
	  echo '</plist>'; \
	} > ${.TARGET}

${SDK_DIR}/info.ini:
	@mkdir -p ${.TARGET:H}
	@{ \
	  echo '[SDK]'; \
	  echo 'name = ${XT_SDK}'; \
	  echo 'version = ${XT_SDK_VERSION}'; \
	  echo 'toolchain = ${XT_TOOLCHAIN}'; \
	  echo 'default_arch = ${XT_DEFAULT_ARCH}'; \
	  echo 'macosx_deployment_target = ${XT_DEPLOYMENT_TARGET}'; \
	} > ${.TARGET}

# --- compatibility ----------------------------------------------------
#
# Our xcrun still looks for <dev>/SDKs/<name>.sdk and
# <dev>/Toolchains/<name>.toolchain with an info.ini inside.  Modern
# Xcode has neither path -- SDKs live under Platforms/ and the toolchain
# is .xctoolchain -- so these symlinks bridge the two until xcrun learns
# the Apple layout (stage 4).  They are additive: nothing in Apple's
# tree is displaced.

bundle-compat: bundle-dirs bundle-sdk bundle-toolchain
	@ln -sfn ../Platforms/${XT_PLATFORM}.platform/Developer/SDKs/${XT_SDK}.sdk \
		${RELEASE}/SDKs/${XT_SDK}.sdk
	@ln -sfn ${XT_TOOLCHAIN}.xctoolchain \
		${RELEASE}/Toolchains/${XT_TOOLCHAIN}.toolchain

# --- Apple's tool aliases ---------------------------------------------
#
# A stock toolchain points nm and otool at the LLVM implementations,
# keeping the cctools builds beside them as nm-classic and otool-classic
# (which is how mk/progs.mk installs ours).  Recreate those two links
# once the llvm port has supplied the targets; without it the names are
# simply absent rather than wrong.

bundle-aliases: bundle-dirs
.for a t in nm llvm-nm otool llvm-otool
	@if [ -e ${TC_DIR}/usr/bin/${t} ] && [ ! -e ${TC_DIR}/usr/bin/${a} ]; then \
		ln -sfn ${t} ${TC_DIR}/usr/bin/${a}; \
		${ECHO} "alias: ${a} -> ${t}"; \
	 fi
.endfor

# --- toolchain shims --------------------------------------------------
#
# scripts/ holds cc/c++/cpp/clang wrappers that drive the real compiler
# through xcrun.  They locate xcrun relative to their own installed
# location, so they are copied verbatim -- no path rewriting needed.
#
# A shim is only installed when no real tool of that name is present.
# `bundles` runs after `ports`, so once the llvm port has installed a
# genuine clang, dropping the clang shim on top of it would be actively
# harmful: the shim resolves its tool with "xcrun -find $0", which would
# find the shim itself and recurse.  Real tools win; shims fill gaps.

SHIM_BIN=	${TC_DIR}/usr/bin

bundle-shims: bundle-dirs
.for s in cc c++ cpp clang
	@mkdir -p ${SHIM_BIN}
	@if [ -e ${SHIM_BIN}/${s} ]; then \
		${ECHO} "shim: ${s} already provided by a port, not overwriting"; \
	 else \
		cp ${SCRIPTS}/${s}.sh ${SHIM_BIN}/${s} && chmod 755 ${SHIM_BIN}/${s}; \
	 fi
.endfor
	@[ -e ${SHIM_BIN}/clang++ ] || ln -sfn clang ${SHIM_BIN}/clang++
	@mkdir -p ${RELEASE}/usr/bin
	@cp ${SCRIPTS}/xcrun-tool.sh ${RELEASE}/usr/bin/xcrun-tool
	@chmod 755 ${RELEASE}/usr/bin/xcrun-tool

.PHONY: bundles bundle-dirs bundle-toolchain bundle-platform bundle-sdk \
	bundle-compat bundle-shims bundle-config bundle-aliases
