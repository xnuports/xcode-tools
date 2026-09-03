/*
 * xpc/debug.h -- the note a debugger reads after an API misuse.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_DEBUG_H__
#define __XPC_DEBUG_H__

#ifndef __XPC_INDIRECT__
#error "Please #include <xpc/xpc.h> instead of this file directly."
#endif

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

/*
 * Why XPC aborted the process, when it did so because the caller used
 * the API wrongly.  Null if that has not happened.
 */
XPC_EXPORT XPC_WARN_RESULT
const char *_Nullable xpc_debugger_api_misuse_info(void);

XPC_ASSUME_NONNULL_END
__END_DECLS

#endif /* __XPC_DEBUG_H__ */
