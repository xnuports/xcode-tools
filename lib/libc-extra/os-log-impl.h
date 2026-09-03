/*
 * os-log-impl.h -- the userland entry point os/log.h is missing.
 *
 * Appended to the SDK's os/log.h.  The header this tree installs is
 * xnu's, from libkern/os, and that is the kernel's: it declares
 * os_log_t, os_log_create, os_log_type_t and OS_LOG_DEFAULT but not
 * _os_log_impl, which is the function every os_log() call actually
 * lands on in userland.  Apple's userland os/log.h declares it and
 * comes from libtrace, which Apple does not publish.
 *
 * libSystem exports the symbol -- it is in the stub this SDK generates
 * -- so all that is wanted is the declaration.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XNUPORTS_OS_LOG_IMPL_H__
#define __XNUPORTS_OS_LOG_IMPL_H__

__BEGIN_DECLS

/*
 * Emit one already-serialised log record.  dso is the calling image,
 * format the format string, buf the encoded arguments and size its
 * length in bytes.
 */
extern void _os_log_impl(void *dso, os_log_t log, os_log_type_t type,
    const char *format, uint8_t *buf, uint32_t size);

__END_DECLS

#endif /* __XNUPORTS_OS_LOG_IMPL_H__ */
