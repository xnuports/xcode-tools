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
