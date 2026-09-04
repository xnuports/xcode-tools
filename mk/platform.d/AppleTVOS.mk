# tvOS
#
# The device platform.  AppleTVSimulator is separate and not
# emitted here.
XT_PLATFORM=		AppleTVOS
XT_SDK=			AppleTVOS
XT_SDK_INTERNAL=	AppleTVOS.Internal
XT_PLATFORM_ID=		com.apple.platform.appletvos
XT_FAMILY_ID=		appletvos
XT_FAMILY_NAME=		Apple TV
XT_PLATFORM_NAME=	tvOS Platform
XT_DESCRIPTION=		tvOS

# The version the installed Xcode reports for this SDK, so the bundle is
# coherent with what a build would target.  DriverKit tracks its own
# numbering and is a release behind the rest.
XT_SDK_VERSION!=	xcrun --sdk appletvos --show-sdk-version 2>/dev/null || echo 0.0
XT_SDK_CANONICAL=	appletvos${XT_SDK_VERSION}
XT_SDK_INTERNAL_CANONICAL=	appletvos${XT_SDK_VERSION}.internal
XT_DEPLOYMENT_TARGET=	${XT_SDK_VERSION}

XT_SDK_ARCHS=		arm64
