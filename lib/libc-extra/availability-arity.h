/*
 * availability-arity.h -- more platforms per annotation.
 *
 * Appended to the SDK's Availability.h, and it has to be that file
 * rather than AvailabilityInternal.h: Availability.h includes the
 * internal header first and then defines __API_AVAILABLE and friends,
 * so anything defined earlier is simply overwritten.
 *
 * The installed headers dispatch on argument count and stop early --
 * four platforms for __API_AVAILABLE, three for __API_UNAVAILABLE,
 * four (after the message) for the deprecations.  Framework headers
 * name more than that.  Security's SecProtocolMetadata.h annotates
 * with macos, ios, watchos, tvos and macCatalyst together, which is
 * five.
 *
 * Overflowing the dispatcher does not fail cleanly.  __API_UNAVAILABLE
 * with five platforms picks the fourth argument as the macro name and
 * calls it, so the annotation silently becomes something else instead
 * of erroring -- which is worse than the parse error the deprecations
 * give.  Both are fixed by widening the tables to eight.
 *
 * The per-count macros build on __API_A, __API_U, __API_D and __API_R
 * from AvailabilityInternal.h, exactly as the shipped ones do.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XNUPORTS_AVAILABILITY_ARITY_H__
#define __XNUPORTS_AVAILABILITY_ARITY_H__

#if defined(__API_A) && defined(__API_U) && defined(__API_D)

/* Availability. */
#define __API_AVAILABLE5(a,b,c,d,e) \
	__API_AVAILABLE4(a,b,c,d) __API_A(e)
#define __API_AVAILABLE6(a,b,c,d,e,f) \
	__API_AVAILABLE5(a,b,c,d,e) __API_A(f)
#define __API_AVAILABLE7(a,b,c,d,e,f,g) \
	__API_AVAILABLE6(a,b,c,d,e,f) __API_A(g)
#define __API_AVAILABLE8(a,b,c,d,e,f,g,h) \
	__API_AVAILABLE7(a,b,c,d,e,f,g) __API_A(h)

#undef __API_AVAILABLE_GET_MACRO
#define __API_AVAILABLE_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME

#undef __API_AVAILABLE
#define __API_AVAILABLE(...) \
	__API_AVAILABLE_GET_MACRO(__VA_ARGS__, \
	    __API_AVAILABLE8,__API_AVAILABLE7,__API_AVAILABLE6, \
	    __API_AVAILABLE5,__API_AVAILABLE4,__API_AVAILABLE3, \
	    __API_AVAILABLE2,__API_AVAILABLE1)(__VA_ARGS__)

/* Unavailability. */
#define __API_UNAVAILABLE4(a,b,c,d)   __API_UNAVAILABLE3(a,b,c) __API_U(d)
#define __API_UNAVAILABLE5(a,b,c,d,e) __API_UNAVAILABLE4(a,b,c,d) __API_U(e)
#define __API_UNAVAILABLE6(a,b,c,d,e,f) \
	__API_UNAVAILABLE5(a,b,c,d,e) __API_U(f)
#define __API_UNAVAILABLE7(a,b,c,d,e,f,g) \
	__API_UNAVAILABLE6(a,b,c,d,e,f) __API_U(g)
#define __API_UNAVAILABLE8(a,b,c,d,e,f,g,h) \
	__API_UNAVAILABLE7(a,b,c,d,e,f,g) __API_U(h)

#undef __API_UNAVAILABLE_GET_MACRO
#define __API_UNAVAILABLE_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME

#undef __API_UNAVAILABLE
#define __API_UNAVAILABLE(...) \
	__API_UNAVAILABLE_GET_MACRO(__VA_ARGS__, \
	    __API_UNAVAILABLE8,__API_UNAVAILABLE7,__API_UNAVAILABLE6, \
	    __API_UNAVAILABLE5,__API_UNAVAILABLE4,__API_UNAVAILABLE3, \
	    __API_UNAVAILABLE2,__API_UNAVAILABLE1)(__VA_ARGS__)

/*
 * Deprecations.  The first argument is the message or replacement, so
 * MSG6 is five platforms.
 */
#define __API_DEPRECATED_MSG6(m,a,b,c,d,e) \
	__API_DEPRECATED_MSG5(m,a,b,c,d) __API_D(m,e)
#define __API_DEPRECATED_MSG7(m,a,b,c,d,e,f) \
	__API_DEPRECATED_MSG6(m,a,b,c,d,e) __API_D(m,f)
#define __API_DEPRECATED_MSG8(m,a,b,c,d,e,f,g) \
	__API_DEPRECATED_MSG7(m,a,b,c,d,e,f) __API_D(m,g)
#define __API_DEPRECATED_MSG9(m,a,b,c,d,e,f,g,h) \
	__API_DEPRECATED_MSG8(m,a,b,c,d,e,f,g) __API_D(m,h)

#undef __API_DEPRECATED_MSG_GET_MACRO
#define __API_DEPRECATED_MSG_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME

#undef __API_DEPRECATED
#define __API_DEPRECATED(...) \
	__API_DEPRECATED_MSG_GET_MACRO(__VA_ARGS__, \
	    __API_DEPRECATED_MSG9,__API_DEPRECATED_MSG8, \
	    __API_DEPRECATED_MSG7,__API_DEPRECATED_MSG6, \
	    __API_DEPRECATED_MSG5,__API_DEPRECATED_MSG4, \
	    __API_DEPRECATED_MSG3,__API_DEPRECATED_MSG2)(__VA_ARGS__)

#if defined(__API_R)
#define __API_DEPRECATED_REP6(r,a,b,c,d,e) \
	__API_DEPRECATED_REP5(r,a,b,c,d) __API_R(r,e)
#define __API_DEPRECATED_REP7(r,a,b,c,d,e,f) \
	__API_DEPRECATED_REP6(r,a,b,c,d,e) __API_R(r,f)
#define __API_DEPRECATED_REP8(r,a,b,c,d,e,f,g) \
	__API_DEPRECATED_REP7(r,a,b,c,d,e,f) __API_R(r,g)
#define __API_DEPRECATED_REP9(r,a,b,c,d,e,f,g,h) \
	__API_DEPRECATED_REP8(r,a,b,c,d,e,f,g) __API_R(r,h)

#undef __API_DEPRECATED_REP_GET_MACRO
#define __API_DEPRECATED_REP_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME

#undef __API_DEPRECATED_WITH_REPLACEMENT
#define __API_DEPRECATED_WITH_REPLACEMENT(...) \
	__API_DEPRECATED_REP_GET_MACRO(__VA_ARGS__, \
	    __API_DEPRECATED_REP9,__API_DEPRECATED_REP8, \
	    __API_DEPRECATED_REP7,__API_DEPRECATED_REP6, \
	    __API_DEPRECATED_REP5,__API_DEPRECATED_REP4, \
	    __API_DEPRECATED_REP3,__API_DEPRECATED_REP2)(__VA_ARGS__)
#endif /* __API_R */

#endif /* __API_A && __API_U && __API_D */

#endif /* __XNUPORTS_AVAILABILITY_ARITY_H__ */
