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
