/*
 * math.h -- the interface, for a system that does not publish one.
 *
 * This is an adapter, not a math library.  The declarations come from
 * msun/math.h next to it, taken unmodified from FreeBSD so it can be
 * updated by replacing the file; what this adds is the handful of
 * macros that file is written against.
 *
 * msun gates its declarations on FreeBSD's visibility macros --
 * __ISO_C_VISIBLE and friends, set by FreeBSD's <sys/cdefs.h>.  Apple's
 * cdefs.h answers the same question with __DARWIN_C_LEVEL and defines
 * none of them, so msun's math.h parses on this system and declares
 * almost nothing: FP_NAN and the rest sit inside "#if __ISO_C_VISIBLE >=
 * 1999" and never appear.  That is why installing it alone left <cmath>
 * still unable to find them.
 *
 * The values below are the visibility FreeBSD's cdefs.h selects for a
 * default compilation -- C11, POSIX 2008, XSI 7, BSD extensions on --
 * which is what a macOS SDK exposes too.  Each is guarded, so a program
 * that has already chosen its own level keeps it.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _XNUPORTS_MATH_H_
#define _XNUPORTS_MATH_H_

#include <sys/cdefs.h>

#ifndef __ISO_C_VISIBLE
#define __ISO_C_VISIBLE		2011
#endif
#ifndef __POSIX_VISIBLE
#define __POSIX_VISIBLE		200809
#endif
#ifndef __XSI_VISIBLE
#define __XSI_VISIBLE		700
#endif
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE		1
#endif
#ifndef __EXT1_VISIBLE
#define __EXT1_VISIBLE		1
#endif

/*
 * The evaluation types.  FreeBSD declares __float_t and __double_t in
 * <machine/_types.h>; Apple's has no such thing, so they are derived
 * here from the compiler's own account of how it evaluates floating
 * point, which is what the C standard says they mean.
 */
#ifndef __FLT_EVAL_METHOD__
#define __FLT_EVAL_METHOD__	0
#endif

#if __FLT_EVAL_METHOD__ == 0
typedef float		__float_t;
typedef double		__double_t;
#elif __FLT_EVAL_METHOD__ == 1
typedef double		__float_t;
typedef double		__double_t;
#else
typedef long double	__float_t;
typedef long double	__double_t;
#endif

#include <msun/math.h>

#endif /* _XNUPORTS_MATH_H_ */
