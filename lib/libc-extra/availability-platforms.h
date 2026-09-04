/*
 * availability-platforms.h -- the platform names xnu's Availability
 * headers do not carry.
 *
 * Appended to the SDK's AvailabilityInternal.h rather than included
 * from anywhere.  It has to be in that header specifically: the
 * annotations are expanded wherever <Availability.h> is included, and
 * mach-o/dyld.h includes that and nothing else before saying
 * __API_UNAVAILABLE(bridgeos).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* marker for the SDK's idempotent append: __XNUPORTS_AVAILABILITY_PLATFORMS__ */

/*
 * Platforms xnu's Availability headers do not name.
 *
 * That set knows ios, macos, macosx, tvos and watchos and stops there,
 * while headers in this SDK annotate for more -- mach-o/dyld.h says
 * __API_UNAVAILABLE(bridgeos) and does not compile without it.  Each
 * expands the same way the five do: a platform name and a clause the
 * availability attribute understands.  clang knows all of these names;
 * it is only this generation of the headers that does not.
 */
#define __API_AVAILABLE_PLATFORM_bridgeos(x)        bridgeos,introduced=x
#define __API_AVAILABLE_PLATFORM_driverkit(x)       driverkit,introduced=x
#define __API_AVAILABLE_PLATFORM_visionos(x)        visionos,introduced=x
#define __API_AVAILABLE_PLATFORM_xros(x)            visionos,introduced=x
#define __API_AVAILABLE_PLATFORM_maccatalyst(x)     maccatalyst,introduced=x

#define __API_UNAVAILABLE_PLATFORM_bridgeos         bridgeos,unavailable
#define __API_UNAVAILABLE_PLATFORM_driverkit        driverkit,unavailable
#define __API_UNAVAILABLE_PLATFORM_visionos         visionos,unavailable
#define __API_UNAVAILABLE_PLATFORM_xros             visionos,unavailable
#define __API_UNAVAILABLE_PLATFORM_maccatalyst      maccatalyst,unavailable

#define __API_DEPRECATED_PLATFORM_bridgeos(x,y)     bridgeos,introduced=x,deprecated=y
#define __API_DEPRECATED_PLATFORM_driverkit(x,y)    driverkit,introduced=x,deprecated=y
#define __API_DEPRECATED_PLATFORM_visionos(x,y)     visionos,introduced=x,deprecated=y
#define __API_DEPRECATED_PLATFORM_xros(x,y)         visionos,introduced=x,deprecated=y
#define __API_DEPRECATED_PLATFORM_maccatalyst(x,y)  maccatalyst,introduced=x,deprecated=y

/*
 * Apple's own spellings.
 *
 * The set above is what xnu's headers reach for.  Framework headers
 * reach for more, and for different spellings of the same platform:
 * Security's SecBase.h says API_UNAVAILABLE(macCatalyst) with a
 * capital C, which is a different macro name from the maccatalyst
 * above and expands to nothing without this.  These are the names
 * Apple's own AvailabilityInternal.h defines, less the ones already
 * covered.
 */
#define __API_AVAILABLE_PLATFORM_macCatalyst(x)     macCatalyst,introduced=x
#define __API_UNAVAILABLE_PLATFORM_macCatalyst      macCatalyst,unavailable
#define __API_DEPRECATED_PLATFORM_macCatalyst(x,y)  macCatalyst,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_kernelkit(x)       kernelkit,introduced=x
#define __API_UNAVAILABLE_PLATFORM_kernelkit        kernelkit,unavailable
#define __API_DEPRECATED_PLATFORM_kernelkit(x,y)    kernelkit,introduced=x,deprecated=y

/*
 * The application-extension variants.  A header that annotates for
 * them names them exactly like any other platform.
 */
#define __API_AVAILABLE_PLATFORM_iOSApplicationExtension(x)     ios_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_iOSApplicationExtension      ios_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_iOSApplicationExtension(x,y)  ios_app_extension,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_macOSApplicationExtension(x)     macos_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_macOSApplicationExtension      macos_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_macOSApplicationExtension(x,y)  macos_app_extension,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_tvOSApplicationExtension(x)     tvos_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_tvOSApplicationExtension      tvos_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_tvOSApplicationExtension(x,y)  tvos_app_extension,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_watchOSApplicationExtension(x)     watchos_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_watchOSApplicationExtension      watchos_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_watchOSApplicationExtension(x,y)  watchos_app_extension,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_visionOSApplicationExtension(x)     visionos_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_visionOSApplicationExtension      visionos_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_visionOSApplicationExtension(x,y)  visionos_app_extension,introduced=x,deprecated=y

#define __API_AVAILABLE_PLATFORM_macCatalystApplicationExtension(x)     maccatalyst_app_extension,introduced=x
#define __API_UNAVAILABLE_PLATFORM_macCatalystApplicationExtension      maccatalyst_app_extension,unavailable
#define __API_DEPRECATED_PLATFORM_macCatalystApplicationExtension(x,y)  maccatalyst_app_extension,introduced=x,deprecated=y

/*
 * The version constants.
 *
 * Apple's Availability.h includes <AvailabilityVersions.h>; the copy
 * this SDK installs does not, so __MAC_10_13_4 and its neighbours are
 * undefined even though the header defining them is right there.
 * Security's SecAccess.h says __OSX_AVAILABLE_STARTING(__MAC_10_13_4,
 * __IPHONE_NA) and does not parse without them.
 */
#if __has_include(<AvailabilityVersions.h>)
#include <AvailabilityVersions.h>
#endif

/*
 * And the legacy internal macros.  __OSX_AVAILABLE_STARTING expands to
 * __AVAILABILITY_INTERNAL__MAC_10_13_4 and its kin, which live in
 * AvailabilityInternalLegacy.h -- installed here, but included by
 * nothing, so the expansion left an undefined identifier behind.
 * Apple's Availability.h includes it right after the versions.
 */
#if __has_include(<AvailabilityInternalLegacy.h>)
#include <AvailabilityInternalLegacy.h>
#endif

/*
 * __IPHONE_NA is not in that header either.  It is the "not available
 * on this platform" sentinel, and Apple's value for it is 9999.
 */
#ifndef __IPHONE_NA
#define __IPHONE_NA 9999
#endif
