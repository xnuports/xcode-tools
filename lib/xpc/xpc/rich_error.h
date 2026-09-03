/*
 * xpc/rich_error.h -- an error with a description attached.
 *
 * The newer interfaces -- sessions and listeners -- report failure with
 * one of these rather than with an error dictionary.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_RICH_ERROR_H__
#define __XPC_RICH_ERROR_H__

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

XPC_DECL(xpc_rich_error);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const char *xpc_rich_error_copy_description(xpc_rich_error_t error);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
bool xpc_rich_error_can_retry(xpc_rich_error_t error);

XPC_ASSUME_NONNULL_END
__END_DECLS

#endif /* __XPC_RICH_ERROR_H__ */
