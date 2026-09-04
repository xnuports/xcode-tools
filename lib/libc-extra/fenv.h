/*
 * fenv.h -- the C99 floating-point environment.
 *
 * Apple ships this in usr/include and it belongs to Libm.  Apple's
 * published Libm cannot supply it: that drop last changed in July
 * 2011, its Source/fenv.h dispatches on __ppc__, __i386__ and __arm__
 * and #errors on anything else, and its ARM header guards its own body
 * on __arm__ as well -- so on arm64 it defines nothing at all, not even
 * fenv_t.  __arm64__ appears nowhere in the drop.  Using its 32-bit ARM
 * definitions would be worse than not having them: that fenv_t is a
 * 4-byte FPSCR union where this architecture's is 16 bytes, so
 * fegetenv() would write past the end of the caller's object.
 *
 * So it is reconstructed, and the numbers are measured rather than
 * remembered.  A program compiled against the real header printed
 * every macro and the sizes on both architectures, and those are the
 * values below: fenv_t is 16 bytes either way, fexcept_t is 2, and the
 * exception bits differ between them -- FE_INEXACT is 0x10 on arm64
 * and 0x20 on x86_64, and each has a sixth flag the other does not.
 *
 * The functions are the eleven C99 names, all of them in libSystem.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FENV_H__
#define __FENV_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

#if defined(__arm64__) || defined(__aarch64__)

typedef struct {
	unsigned long long	__fpsr;		/* status register  */
	unsigned long long	__fpcr;		/* control register */
} fenv_t;

typedef unsigned short fexcept_t;

#define FE_INEXACT		0x0010
#define FE_UNDERFLOW		0x0008
#define FE_OVERFLOW		0x0004
#define FE_DIVBYZERO		0x0002
#define FE_INVALID		0x0001

/* Not in C99: set when a denormal is flushed to zero. */
#define FE_FLUSHTOZERO		0x0080

#define FE_ALL_EXCEPT		0x009f

#define FE_TONEAREST		0x00000000
#define FE_UPWARD		0x00400000
#define FE_DOWNWARD		0x00800000
#define FE_TOWARDZERO		0x00c00000

#elif defined(__i386__) || defined(__x86_64__)

typedef struct {
	unsigned short		__control;	/* x87 control word	     */
	unsigned short		__status;	/* x87 status word	     */
	unsigned int		__mxcsr;	/* SSE status/control	     */
	char			__reserved[8];
} fenv_t;

typedef unsigned short fexcept_t;

#define FE_INEXACT		0x0020
#define FE_UNDERFLOW		0x0010
#define FE_OVERFLOW		0x0008
#define FE_DIVBYZERO		0x0004
#define FE_INVALID		0x0001

/* Not in C99: the x87 denormal-operand exception. */
#define FE_DENORMALOPERAND	0x0002

#define FE_ALL_EXCEPT		0x003f

#define FE_TONEAREST		0x0000
#define FE_DOWNWARD		0x0400
#define FE_UPWARD		0x0800
#define FE_TOWARDZERO		0x0c00

#else
#error fenv.h: unknown architecture
#endif

/* The default environment, as installed at program start. */
extern const fenv_t _FE_DFL_ENV;
#define FE_DFL_ENV		&_FE_DFL_ENV

/* Exception flags. */
extern int feclearexcept(int /* excepts */);
extern int fegetexceptflag(fexcept_t * /* flagp */, int /* excepts */);
extern int feraiseexcept(int /* excepts */);
extern int fesetexceptflag(const fexcept_t * /* flagp */, int /* excepts */);
extern int fetestexcept(int /* excepts */);

/* Rounding direction. */
extern int fegetround(void);
extern int fesetround(int /* round */);

/* The whole environment. */
extern int fegetenv(fenv_t * /* envp */);
extern int feholdexcept(fenv_t * /* envp */);
extern int fesetenv(const fenv_t * /* envp */);
extern int feupdateenv(const fenv_t * /* envp */);

__END_DECLS

#endif /* __FENV_H__ */
