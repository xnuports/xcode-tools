/*
 * complex.h -- the interface, for a system that does not publish one.
 *
 * The same adapter math-darwin.h is, for the same reason and against
 * the same file.  complex.h next to it is FreeBSD's, taken unmodified
 * so it can be updated by replacing the file; this adds the one macro
 * that file is written against.
 *
 * FreeBSD's complex.h puts CMPLX, CMPLXF and CMPLXL behind "#if
 * __ISO_C_VISIBLE >= 2011", a macro FreeBSD's <sys/cdefs.h> sets and
 * Apple's does not define at all.  Included as it stands on this
 * system it therefore parses and declares the functions but none of
 * the C11 macros -- the same silent half-result installing msun's
 * math.h alone produced.
 *
 * The value below is the visibility FreeBSD's cdefs.h selects for a
 * default compilation, which is what a macOS SDK exposes too.  It is
 * guarded, so a program that has already chosen its own level keeps
 * it.  Nothing else is needed here: __pure2 comes from Apple's own
 * cdefs.h, and __generic is used only behind #ifdef.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _XNUPORTS_COMPLEX_H_
#define _XNUPORTS_COMPLEX_H_

#include <sys/cdefs.h>

#ifndef __ISO_C_VISIBLE
#define __ISO_C_VISIBLE		2011
#endif

#include <msun/complex.h>

#endif /* _XNUPORTS_COMPLEX_H_ */
