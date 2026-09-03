/*
 * xpc/connection.h -- a channel to another process.
 *
 * A connection is made, given an event handler, resumed, and then sent
 * messages; replies arrive on a queue or through a callback.  This is
 * the interface most code that uses XPC actually calls.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_CONNECTION_H__
#define __XPC_CONNECTION_H__

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

#include <sys/types.h>
#include <bsm/audit.h>

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

/*
 * Flags for xpc_connection_create_mach_service().  A listener takes
 * connections rather than making them; a privileged connection reaches
 * the privileged bootstrap.
 */
#define XPC_CONNECTION_MACH_SERVICE_LISTENER    (1 << 0)
#define XPC_CONNECTION_MACH_SERVICE_PRIVILEGED  (1 << 1)

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_connection_t xpc_connection_create(const char *_Nullable name,
    dispatch_queue_t _Nullable targetq);

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_connection_t xpc_connection_create_mach_service(const char *name,
    dispatch_queue_t _Nullable targetq, uint64_t flags);

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_connection_t xpc_connection_create_from_endpoint(xpc_endpoint_t endpoint);

XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_target_queue(xpc_connection_t connection,
    dispatch_queue_t _Nullable targetq);

#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_set_event_handler(xpc_connection_t connection,
    xpc_handler_t handler);
#endif

XPC_EXPORT XPC_NONNULL1
void xpc_connection_activate(xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL1
void xpc_connection_suspend(xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL1
void xpc_connection_resume(xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL1
void xpc_connection_cancel(xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_send_message(xpc_connection_t connection,
    xpc_object_t message);

XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_send_barrier(xpc_connection_t connection,
    dispatch_block_t barrier);

#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL4
void xpc_connection_send_message_with_reply(xpc_connection_t connection,
    xpc_object_t message, dispatch_queue_t _Nullable replyq,
    xpc_handler_t handler);
#endif

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t xpc_connection_send_message_with_reply_sync(
    xpc_connection_t connection, xpc_object_t message);

/* Who is on the other end. */
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
const char *_Nullable xpc_connection_get_name(xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
uid_t xpc_connection_get_euid(xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
gid_t xpc_connection_get_egid(xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
pid_t xpc_connection_get_pid(xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
au_asid_t xpc_connection_get_asid(xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_context(xpc_connection_t connection,
    void *_Nullable context);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
void *_Nullable xpc_connection_get_context(xpc_connection_t connection);

#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_finalizer_f(xpc_connection_t connection,
    void (*_Nullable finalizer)(void *_Nullable value));
#endif

XPC_ASSUME_NONNULL_END
__END_DECLS

#endif /* __XPC_CONNECTION_H__ */
