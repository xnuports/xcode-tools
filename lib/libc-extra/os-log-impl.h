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
 * The signature below was read off a disassembly.  PureDarwin's Libtrace,
 * which reimplements os_log for a Darwin that has no libSystem to borrow
 * from, defines it with the same six parameters in the same order under the
 * same names, which is a second and independent source for it.
 *
 * That project is not vendored here.  It supplies no public os/log.h -- the
 * one thing missing -- and its implementation is one this tree cannot use:
 * it logs through ASL, which is a working stand-in on a system without
 * libtrace but is not what Apple's does, and it has no
 * _os_log_send_and_compose_impl, which Apple's git needs.  On macOS the real
 * implementation is already under the SDK's stub.
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

/*
 * Whether a message of this type would be recorded.  os/log.h uses it in
 * os_log_debug_enabled() and friends, and os/log_private.h needs it to
 * decide whether to set OS_LOG_F_SEND, but the kernel header this tree
 * installs never declares it.
 */
extern bool os_log_type_enabled(os_log_t oslog, os_log_type_t type);

__END_DECLS

#endif /* __XNUPORTS_OS_LOG_IMPL_H__ */
