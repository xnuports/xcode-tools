/*
 * os/log_private.h -- the userspace variant.
 *
 * Not to be confused with xnu's libkern/os/log_private.h, which is the
 * kernel one and carries none of this.  The userspace header lives in
 * libtrace, which Apple does not publish, so what follows is
 * reconstructed rather than copied.
 *
 * os/assumes.h needs it: with OS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE
 * defined, os_crash() composes its message through
 * os_log_send_and_compose().  Apple's git defines that macro in
 * usage.c, so their git cannot build without this header.
 *
 * mach/kern_return.h is included because os/assumes.h declares
 * os_assert_mach() with a kern_return_t inside the same block this
 * header unlocks, and nothing else in that block brings the type in.
 *
 * The signature below is not guesswork.  Apple ships the compiled
 * result -- Xcode's git is "2.50.1 (Apple Git-155)", the same version
 * this tree builds -- and it carries exactly one call to
 * _os_log_send_and_compose_impl.  Disassembling it gives the argument
 * order directly:
 *
 *	w0	flags, computed as 2, or 3 when os_log_type_enabled()
 *		returns true -- so COMPOSE is 2 and always set, SEND is 1
 *		and set only when the type is enabled
 *	x1	&fmt_out, a zeroed const char * slot
 *	x2	the compose buffer
 *	w3	its size (0x50, matching OS_CRASH_MSG_BUFSZ of 80)
 *	x4	the Mach-O header address, i.e. &__dso_handle
 *	x5	__os_log_default
 *	w6	0x10, OS_LOG_TYPE_ERROR
 *	x7	the format string
 *	[sp]	the __builtin_os_log_format buffer
 *	[sp+8]	its size
 *
 * Note the dso argument: the macro inserts it after the buffer size,
 * where assumes.h's call site passes the log object.  The return value
 * is the composed string, which assumes.h hands to _os_crash_msg().
 */

#ifndef __OS_LOG_PRIVATE_H__
#define __OS_LOG_PRIVATE_H__

#include <mach/kern_return.h>
#include <os/base.h>
#include <os/log.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/*!
 * @typedef os_log_pack_t
 *
 * @discussion
 * A packed log message.  os/assumes.h declares _os_crash_fmt() in terms
 * of it, so the name has to exist; the layout is deliberately left
 * opaque because nothing in this tree allocates one -- doing that needs
 * os_log_pack_size()/os_log_pack_fill(), and their layout contract is
 * not recoverable from the shipped binaries the way the call above was.
 */
typedef struct os_log_pack_s *os_log_pack_t;

/*!
 * @enum os_log_send_and_compose flags
 *
 * @constant OS_LOG_F_SEND
 * Emit the message to the log system.
 *
 * @constant OS_LOG_F_COMPOSE
 * Format the message into the caller's buffer and return it.
 */
#define OS_LOG_F_SEND		0x01u
#define OS_LOG_F_COMPOSE	0x02u

API_AVAILABLE(macos(13.0), ios(16.0), tvos(16.0), watchos(9.0))
extern char *
_os_log_send_and_compose_impl(uint32_t flags, const char **fmt_out,
    char *buf, size_t buf_size, void *dso, os_log_t log,
    os_log_type_t type, const char *format, uint8_t *fmt_buf,
    uint32_t fmt_buf_size);

/*!
 * @function os_log_send_and_compose
 *
 * @abstract
 * Send a log message and compose it into a caller-supplied buffer.
 *
 * @discussion
 * Mirrors os_log_with_type(): the format string is placed in
 * __TEXT,__os_log and the arguments are packed by the compiler with
 * __builtin_os_log_format.  OS_LOG_F_SEND is cleared when the type is
 * not enabled, which is what the shipped code does rather than
 * skipping the call.
 *
 * @result
 * The composed message, or NULL.  fmt_out receives the format string.
 */
#define os_log_send_and_compose(flags, fmt_out, buf, buf_size, log, type, format, ...) \
	__extension__({								\
	    _Static_assert(__builtin_constant_p(format),			\
		"format string must be constant");				\
	    __attribute__((section("__TEXT,__os_log")))				\
	    static const char _os_slc_fmt[] = format;				\
	    uint8_t _os_slc_buf[__builtin_os_log_format_buffer_size(format,	\
		##__VA_ARGS__)];						\
	    os_log_t _os_slc_log = (log);					\
	    os_log_type_t _os_slc_type = (type);				\
	    uint32_t _os_slc_flags = (uint32_t)(flags);				\
	    if (!os_log_type_enabled(_os_slc_log, _os_slc_type)) {		\
		    _os_slc_flags &= ~OS_LOG_F_SEND;				\
	    }									\
	    _os_log_send_and_compose_impl(_os_slc_flags, (fmt_out), (buf),	\
		(buf_size), &__dso_handle, _os_slc_log, _os_slc_type,		\
		_os_slc_fmt,							\
		(uint8_t *)__builtin_os_log_format(_os_slc_buf, format,		\
		    ##__VA_ARGS__),						\
		(uint32_t)sizeof(_os_slc_buf));					\
	})

__END_DECLS

#endif /* __OS_LOG_PRIVATE_H__ */
