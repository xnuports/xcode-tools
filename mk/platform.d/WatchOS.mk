# watchOS
#
# The device platform.  arm64_32 is the watch's own ABI --
# 64-bit registers with 32-bit pointers -- and comes first because it is
# what the hardware runs.  WatchSimulator is separate.
XT_PLATFORM=		WatchOS
XT_SDK=			WatchOS
XT_SDK_INTERNAL=	WatchOS.Internal
XT_PLATFORM_ID=		com.apple.platform.watchos
XT_FAMILY_ID=		watchos
XT_FAMILY_NAME=		Apple Watch
XT_PLATFORM_NAME=	watchOS Platform
XT_DESCRIPTION=		watchOS

# The version the installed Xcode reports for this SDK, so the bundle is
# coherent with what a build would target.  DriverKit tracks its own
# numbering and is a release behind the rest.
XT_SDK_VERSION!=	xcrun --sdk watchos --show-sdk-version 2>/dev/null || echo 0.0
XT_SDK_CANONICAL=	watchos${XT_SDK_VERSION}
XT_SDK_INTERNAL_CANONICAL=	watchos${XT_SDK_VERSION}.internal
XT_DEPLOYMENT_TARGET=	${XT_SDK_VERSION}

XT_SDK_ARCHS=		arm64_32 arm64

# The C headers are the macOS ones restricted to what this platform may use,
# so they are installed out of the MacOSX SDK this build has already
# assembled rather than assembled again.  lib/watchos-headers.txt is the
# restriction.  Unlike DriverKit this SDK's content sits at the top of the
# bundle, as macOS's does, so there is no XT_SDK_ROOT.
XT_SDK_HEADERS_CMD=	${TOP}/mk/scripts/install-sdk-headers.sh WatchOS \
			${TOP}/lib/watchos-headers.txt \
			${RELEASE}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include \
			${SDK_CONTENT_DIR}/usr/include
