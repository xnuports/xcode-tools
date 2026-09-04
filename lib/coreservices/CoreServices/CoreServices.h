/*
 * CoreServices.h -- the umbrella, as far as this tree provides it.
 *
 * Apple's CoreServices is an umbrella over nine subframeworks: AE,
 * CarbonCore, DictionaryServices, FSEvents, LaunchServices, Metadata,
 * OSServices, SearchKit and SharedFileList.  None of them is open
 * source.  This tree carries FSEvents, because that is the one a
 * developer tool actually needs -- git's fsmonitor watches a work tree
 * through it -- and declares nothing it cannot provide.
 *
 * So this header is deliberately smaller than Apple's.  Code that
 * includes <CoreServices/CoreServices.h> for FSEvents compiles against
 * it unchanged; code that wants LaunchServices or Metadata does not,
 * and should fail at the include rather than at some confusing point
 * later.  The remaining subframeworks can be added here as they are
 * reconstructed.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CORESERVICES_H__
#define __CORESERVICES_H__

#include <CoreFoundation/CoreFoundation.h>
#include <FSEvents/FSEvents.h>

#endif /* __CORESERVICES_H__ */
