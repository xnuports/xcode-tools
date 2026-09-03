/*
 * xpc/base.h -- the attributes and visibility macros the XPC headers
 * are written in.
 *
 * Every declaration in the other headers is spelled with these, so this
 * one carries no interface of its own.  The definitions are the ones
 * the API is documented against; where a compiler does not support an
 * attribute the macro expands to nothing, which is what Apple's does
 * too.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_BASE_H__
#define __XPC_BASE_H__

#include <sys/cdefs.h>

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

#include <xpc/availability.h>

#if __has_feature(assume_nonnull)
#define XPC_ASSUME_NONNULL_BEGIN  _Pragma("clang assume_nonnull begin")
#define XPC_ASSUME_NONNULL_END    _Pragma("clang assume_nonnull end")
#else
#define XPC_ASSUME_NONNULL_BEGIN
#define XPC_ASSUME_NONNULL_END
#endif

#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#define XPC_EXPORT              __attribute__((__visibility__("default")))
#define XPC_INLINE              static __inline__ __attribute__((__always_inline__))
#define XPC_NOEXPORT            __attribute__((__visibility__("hidden")))
#define XPC_NOTHROW             __attribute__((__nothrow__))
#define XPC_NONNULL_ALL         __attribute__((__nonnull__))
#define XPC_NONNULL(...)        __attribute__((__nonnull__(__VA_ARGS__)))

/*
 * The numbered forms.  The declarations are written with these -- the
 * first argument must not be null, the second, and so on -- which reads
 * better at a call site than counting commas in XPC_NONNULL(1, 3).
 */
#define XPC_NONNULL1            XPC_NONNULL(1)
#define XPC_NONNULL2            XPC_NONNULL(2)
#define XPC_NONNULL3            XPC_NONNULL(3)
#define XPC_NONNULL4            XPC_NONNULL(4)
#define XPC_NONNULL5            XPC_NONNULL(5)
#define XPC_NONNULL6            XPC_NONNULL(6)

#if __has_attribute(noescape)
#define XPC_NOESCAPE            __attribute__((__noescape__))
#else
#define XPC_NOESCAPE
#endif
#define XPC_NORETURN            __attribute__((__noreturn__))
#define XPC_PRINTF(f, a)        __attribute__((format(printf, f, a)))
#define XPC_WARN_RESULT         __attribute__((__warn_unused_result__))
#define XPC_MALLOC              __attribute__((__malloc__))
#define XPC_UNUSED              __attribute__((__unused__))
#define XPC_SENTINEL            __attribute__((__sentinel__))
#define XPC_PURE                __attribute__((__pure__))
#define XPC_CONSTRUCTOR         __attribute__((constructor))

#define XPC_TRANSPARENT_UNION   __attribute__((__transparent_union__))
#define XPC_DEPRECATED          __attribute__((deprecated))

/*
 * Bridging between XPC's os_object types and Objective-C, when the
 * compiler is compiling Objective-C with ARC and the objects are real
 * Objective-C objects.  Outside that they expand to nothing.
 */
#if OS_OBJECT_USE_OBJC
#define XPC_BRIDGE              OS_OBJECT_BRIDGE
#define XPC_BRIDGEREF_BEGIN(x)
#define XPC_BRIDGEREF_BEGIN_WITH_REF(x, y)
#define XPC_BRIDGEREF_MIDDLE(x)
#define XPC_BRIDGEREF_END(x)
#else
#define XPC_BRIDGE
#define XPC_BRIDGEREF_BEGIN(x)
#define XPC_BRIDGEREF_BEGIN_WITH_REF(x, y)
#define XPC_BRIDGEREF_MIDDLE(x)
#define XPC_BRIDGEREF_END(x)
#endif

#define XPC_CSTRING             const char *
#define XPC_DEBUGGER_EXCL
#define XPC_COUNTEDBY(x)

/*
 * An enumeration whose underlying type is fixed, so its size is part of
 * the ABI rather than left to the compiler.
 */
#define XPC_ENUM(name, type) \
	typedef type name##_t; \
	enum name

#define XPC_SWIFT_NOEXPORT
#define XPC_SWIFT_NORETURN
#define XPC_SWIFT_UNAVAILABLE(m)  __attribute__((availability(swift, unavailable, message=m)))

#endif /* __XPC_BASE_H__ */
