# DriverKit
#
# Userspace drivers.  It has no simulator, and its SDK version
# tracks its own numbering rather than the OS release.
XT_PLATFORM=		DriverKit
XT_SDK=			DriverKit
XT_SDK_INTERNAL=	DriverKit.Internal
XT_PLATFORM_ID=		com.apple.platform.driverkit
XT_FAMILY_ID=		driverkit
XT_FAMILY_NAME=		DriverKit
XT_PLATFORM_NAME=	DriverKit Platform
XT_DESCRIPTION=		DriverKit

# The version the installed Xcode reports for this SDK, so the bundle is
# coherent with what a build would target.  DriverKit tracks its own
# numbering and is a release behind the rest.
XT_SDK_VERSION!=	xcrun --sdk driverkit --show-sdk-version 2>/dev/null || echo 0.0
XT_SDK_CANONICAL=	driverkit${XT_SDK_VERSION}
XT_SDK_INTERNAL_CANONICAL=	driverkit${XT_SDK_VERSION}.internal
XT_DEPLOYMENT_TARGET=	${XT_SDK_VERSION}

XT_SDK_ARCHS=		arm64 x86_64

# A DriverKit SDK describes a runtime rooted at /System/DriverKit rather than
# at /, and is laid out to match: usr/ and System/ sit under System/DriverKit
# inside the .sdk, and there is nothing at the top but SDKSettings.
XT_SDK_ROOT=		System/DriverKit

# The C headers are the macOS ones restricted to what a driver may use, so
# they are installed out of the MacOSX SDK this build has already assembled
# rather than assembled again.  lib/driverkit-headers.txt is the restriction.
XT_SDK_HEADERS_CMD=	${TOP}/mk/scripts/install-driverkit-headers.sh \
			${TOP}/lib/driverkit-headers.txt \
			${RELEASE}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include \
			${SDK_CONTENT_DIR}/usr/include

# The frameworks.  Not class-dumped: a DriverKit framework header is a C++
# class declaration under Apple's OSReference licence, so it comes from the
# same open-source drops the driver families do.  Only some of them have been
# released; the script says which have not.
XT_SDK_FRAMEWORKS_CMD=	${TOP}/mk/scripts/install-driverkit-frameworks.sh \
			${TOP}/lib/driverkit-frameworks.txt \
			${TOP}/src/apple \
			${SDK_CONTENT_DIR}/System/Library/Frameworks
