/*
 * xpc/endpoint.h -- a transferable reference to a connection.
 *
 * An endpoint can be put in a message and sent to another process,
 * which turns it back into a connection to the same listener.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_ENDPOINT_H__
#define __XPC_ENDPOINT_H__

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

XPC_DECL(xpc_endpoint);

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_endpoint_t xpc_endpoint_create(xpc_connection_t connection);

XPC_ASSUME_NONNULL_END
__END_DECLS

#endif /* __XPC_ENDPOINT_H__ */
