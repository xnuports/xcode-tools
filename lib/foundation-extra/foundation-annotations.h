/*
 * foundation-annotations.h -- the NS_ annotation macros the
 * reimplementation does not define yet.
 *
 * Appended to the SDK's NSObjCRuntime.h, the way cf-nullability.h is
 * appended to CFAvailability.h: src/puredarwin/Foundation is a
 * submodule and cannot be edited from here, and every Foundation
 * header reaches NSObjCRuntime.h.
 *
 * These are annotations and nothing else -- availability, nullability
 * and the Swift bridging hints.  They carry no behaviour, but a header
 * naming one that is not defined does not parse, and that is what
 * stopped WebKit: its WEBKIT_CLASS_DEPRECATED_MAC expands to
 * NS_CLASS_DEPRECATED_MAC, so every class declared with it was a
 * syntax error.
 *
 * The expansions match what Apple's produce, checked by preprocessing
 * the same line against their SDK and this one.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XNUPORTS_FOUNDATION_ANNOTATIONS_H__
#define __XNUPORTS_FOUNDATION_ANNOTATIONS_H__

#include <os/availability.h>

/* Nullability, and the audit form that wraps a whole header. */
#if __has_feature(assume_nonnull)
#define NS_ASSUME_NONNULL_BEGIN     _Pragma("clang assume_nonnull begin")
#define NS_ASSUME_NONNULL_END       _Pragma("clang assume_nonnull end")
#else
#define NS_ASSUME_NONNULL_BEGIN
#define NS_ASSUME_NONNULL_END
#endif

#define NS_HEADER_AUDIT_BEGIN(...)  NS_ASSUME_NONNULL_BEGIN
#define NS_HEADER_AUDIT_END(...)    NS_ASSUME_NONNULL_END

/*
 * Availability.  Apple's NS_CLASS_DEPRECATED_MAC expands to a
 * visibility attribute beside the availability one -- a class has to
 * be exported to be subclassed -- and the message argument is always
 * present, empty when the caller gives none.
 */
#if __has_attribute(availability)

#define NS_CLASS_AVAILABLE(_mac, _ios) \
	__attribute__((visibility("default"))) \
	__attribute__((availability(macosx,introduced=_mac)))
#define NS_CLASS_AVAILABLE_MAC(_mac) \
	__attribute__((visibility("default"))) \
	__attribute__((availability(macosx,introduced=_mac)))
#define NS_CLASS_DEPRECATED_MAC(_macIntro, _macDep, ...) \
	__attribute__((visibility("default"))) \
	__attribute__((availability(macosx,introduced=_macIntro,deprecated=_macDep,message="" __VA_ARGS__)))

#define NS_UNAVAILABLE              __attribute__((unavailable))

#else

#define NS_CLASS_AVAILABLE(_mac, _ios)
#define NS_CLASS_AVAILABLE_MAC(_mac)
#define NS_CLASS_DEPRECATED_MAC(_macIntro, _macDep, ...)
#define NS_UNAVAILABLE

#endif

/*
 * An error-code enum: a typed enum whose values are error codes, which
 * for C is an ordinary fixed-underlying-type enum.
 */
#define NS_ERROR_ENUM(_type, _name) \
	enum _name : _type _name; enum __attribute__((ns_error_domain(_name))) _name : _type

/* The Swift-facing hints.  None of them means anything to C. */
#if __has_attribute(swift_private)
#define NS_REFINED_FOR_SWIFT        __attribute__((swift_private))
#else
#define NS_REFINED_FOR_SWIFT
#endif

#if __has_attribute(swift_name)
#define NS_SWIFT_NAME(_name)        __attribute__((swift_name(#_name)))
#else
#define NS_SWIFT_NAME(_name)
#endif

#if __has_attribute(swift_async)
#define NS_SWIFT_ASYNC(_index)      __attribute__((swift_async(not_swift_private, _index)))
#else
#define NS_SWIFT_ASYNC(_index)
#endif

#if __has_attribute(swift_async_name)
#define NS_SWIFT_ASYNC_NAME(_name)  __attribute__((swift_async_name(#_name)))
#else
#define NS_SWIFT_ASYNC_NAME(_name)
#endif

#define NS_SWIFT_ASYNC_THROWS_ON_FALSE(_index)
#define NS_SWIFT_NONISOLATED
#define NS_SWIFT_UI_ACTOR

#endif /* __XNUPORTS_FOUNDATION_ANNOTATIONS_H__ */
