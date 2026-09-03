/*
 * xpc/listener.h -- declared, not complete.
 *
 * One of the newer XPC interfaces.  What is here is the type and the
 * calls this tree has needed; the rest of the interface exists in
 * libSystem and is not declared yet.  The README beside this directory
 * says why these were written rather than taken from a reimplementation,
 * and the same reasoning applies to stopping here rather than guessing:
 * a prototype that does not match Apple's would compile, link against
 * the real libxpc, and be wrong only at runtime.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_LISTENER_H__
#define __XPC_LISTENER_H__

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

XPC_DECL(xpc_listener);

XPC_ASSUME_NONNULL_END
__END_DECLS

#endif /* __XPC_LISTENER_H__ */
