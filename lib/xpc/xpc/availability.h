/*
 * xpc/availability.h -- version guards for the XPC interfaces.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_AVAILABILITY_H__
#define __XPC_AVAILABILITY_H__

#include <Availability.h>
#include <os/availability.h>

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

#define XPC_AVAILABLE(...)              API_AVAILABLE(__VA_ARGS__)
#define XPC_DEPRECATED_MSG(...)         API_DEPRECATED(__VA_ARGS__)
#define XPC_UNAVAILABLE(...)            API_UNAVAILABLE(__VA_ARGS__)

#endif /* __XPC_AVAILABILITY_H__ */
