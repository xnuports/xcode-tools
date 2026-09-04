# iOS
#
# The device platform, not the simulator: iPhoneSimulator is a
# separate bundle with its own SDK and is not emitted here.
XT_PLATFORM=		iPhoneOS
XT_SDK=			iPhoneOS
XT_SDK_INTERNAL=	iPhoneOS.Internal
XT_PLATFORM_ID=		com.apple.platform.iphoneos
XT_FAMILY_ID=		iphoneos
XT_FAMILY_NAME=		iPhone
XT_PLATFORM_NAME=	iOS Platform
XT_DESCRIPTION=		iOS

# The version the installed Xcode reports for this SDK, so the bundle is
# coherent with what a build would target.  DriverKit tracks its own
# numbering and is a release behind the rest.
XT_SDK_VERSION!=	xcrun --sdk iphoneos --show-sdk-version 2>/dev/null || echo 0.0
XT_SDK_CANONICAL=	iphoneos${XT_SDK_VERSION}
XT_SDK_INTERNAL_CANONICAL=	iphoneos${XT_SDK_VERSION}.internal
XT_DEPLOYMENT_TARGET=	${XT_SDK_VERSION}

XT_SDK_ARCHS=		arm64 arm64e
