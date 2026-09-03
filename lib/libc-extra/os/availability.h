/*
 * os/availability.h -- the public API_* availability annotations.
 *
 * os/object.h and everything under os/ and dispatch/ is written against
 * the public spellings -- API_AVAILABLE, API_DEPRECATED and their
 * relatives -- while the SDK's Availability headers define only the
 * double-underscore forms they are built from.  This is the adapter
 * between the two, in the same spirit as math-darwin.h next door.
 *
 * It is ours rather than Apple's for a specific reason.  xnu's
 * EXTERNAL_HEADERS carries three of the five Availability headers, and
 * every header in this SDK that comes from xnu's source tree --
 * sys/qos.h above all -- is written against those.  The Availability
 * headers in darwin-xnu-build's fakeroot are a different generation and
 * are not interchangeable with them: install the fakeroot's five and
 * os/log.h starts compiling while sys/qos.h stops, taking the whole
 * Darwin module with it, and the fakeroot has no sys/qos.h to replace
 * it with.  So the source tree's headers stay and this file defines the
 * public macros over what they actually provide.
 *
 * The annotations that have no internal counterpart -- API_OBSOLETED
 * and the attribute-push _BEGIN/_END pairs -- expand to nothing. They
 * carry deprecation diagnostics and no ABI, so a program built against
 * these links exactly as it would otherwise; what it loses is warnings.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __OS_AVAILABILITY__
#define __OS_AVAILABILITY__

#include <Availability.h>
#include <AvailabilityInternal.h>

/* The four the Availability headers give us directly. */
#if defined(__has_feature) && defined(__has_attribute)
 #if __has_attribute(availability)

  #define API_AVAILABLE(...)                    __API_AVAILABLE(__VA_ARGS__)
  #define API_UNAVAILABLE(...)                  __API_UNAVAILABLE(__VA_ARGS__)
  #define API_DEPRECATED(...)                   __API_DEPRECATED(__VA_ARGS__)
  #define API_DEPRECATED_WITH_REPLACEMENT(...)  __API_DEPRECATED_WITH_REPLACEMENT(__VA_ARGS__)

 #else /* !__has_attribute(availability) */

  #define API_AVAILABLE(...)
  #define API_UNAVAILABLE(...)
  #define API_DEPRECATED(...)
  #define API_DEPRECATED_WITH_REPLACEMENT(...)

 #endif
#else
  #define API_AVAILABLE(...)
  #define API_UNAVAILABLE(...)
  #define API_DEPRECATED(...)
  #define API_DEPRECATED_WITH_REPLACEMENT(...)
#endif

/*
 * Obsoletion has no internal form here.  It annotates and does not
 * change what is emitted, so it is dropped rather than approximated
 * with something that would say the wrong thing.
 */
#define API_OBSOLETED(...)
#define API_OBSOLETED_WITH_REPLACEMENT(...)

/*
 * The region forms push and pop a clang attribute across a stretch of
 * declarations.  Defined away in balanced pairs: what each declaration
 * inside them is, is unchanged.
 */
#define API_AVAILABLE_BEGIN(...)
#define API_AVAILABLE_END
#define API_UNAVAILABLE_BEGIN(...)
#define API_UNAVAILABLE_END
#define API_DEPRECATED_BEGIN(...)
#define API_DEPRECATED_END
#define API_DEPRECATED_WITH_REPLACEMENT_BEGIN(...)
#define API_DEPRECATED_WITH_REPLACEMENT_END
#define API_OBSOLETED_BEGIN(...)
#define API_OBSOLETED_END
#define API_OBSOLETED_WITH_REPLACEMENT_BEGIN(...)
#define API_OBSOLETED_WITH_REPLACEMENT_END

/*
 * The sentinel a header uses when it means "some future release".
 * 100000 is the value Apple's own headers carry.
 */
#ifndef __API_TO_BE_DEPRECATED
#define __API_TO_BE_DEPRECATED 100000
#endif

#define API_TO_BE_DEPRECATED            __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_MACOS      __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_IOS        __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_TVOS       __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_WATCHOS    __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_VISIONOS   __API_TO_BE_DEPRECATED
#define API_TO_BE_DEPRECATED_DRIVERKIT  __API_TO_BE_DEPRECATED


#endif /* __OS_AVAILABILITY__ */
