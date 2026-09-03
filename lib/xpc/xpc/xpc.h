/*
 * xpc/xpc.h -- Apple's interprocess communication library.
 *
 * The object model, the container and primitive types, and the calls
 * that make and take them apart.  The implementation is in libSystem;
 * this declares the interface so a compiler can see it.  See the README
 * beside this directory for why it is written rather than taken from
 * one of the existing reimplementations.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __XPC_H__
#define __XPC_H__

#define __XPC_INDIRECT__

#include <os/object.h>
#include <dispatch/dispatch.h>

#include <sys/mman.h>
#include <uuid/uuid.h>
#include <bsm/audit.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <xpc/base.h>

__BEGIN_DECLS
XPC_ASSUME_NONNULL_BEGIN

/*
 * The type of every XPC value.  Dictionaries, arrays, strings and the
 * rest are all this type; xpc_get_type() says which.
 *
 * These are os_objects, the same as dispatch_object_t and os_log_t: when
 * OS_OBJECT_USE_OBJC is on they are Objective-C objects and participate
 * in ARC, and when it is not they are opaque pointers retained and
 * released by hand.  That is the ABI, not a decision made here.
 */
/*
 * OS_OBJECT_DECL_CLASS and not OS_OBJECT_DECL: the latter is defined
 * only in os/object.h's Objective-C branch, where it expands to an
 * @protocol, so a C compilation never sees xpc_object_t at all.
 * OS_OBJECT_DECL_CLASS is defined either way -- an Objective-C class
 * under OS_OBJECT_USE_OBJC and a typedef'd opaque pointer otherwise --
 * and it is what dispatch_object is declared with two headers over.
 */
OS_OBJECT_DECL_CLASS(xpc_object);

#ifndef XPC_DECL
#define XPC_DECL(name) typedef xpc_object_t name##_t
#endif

#define XPC_GLOBAL_OBJECT(object)  ((OS_OBJECT_BRIDGE xpc_object_t)&(object))
#define XPC_RETURNS_RETAINED       OS_OBJECT_RETURNS_RETAINED

/*
 * Declared here rather than in connection.h because the array and
 * dictionary setters below take one, and those come first.
 */
XPC_DECL(xpc_connection);
#ifdef __BLOCKS__
typedef void (^xpc_connection_handler_t)(xpc_connection_t connection);
#endif

/*
 * A type tag.  The singletons below name each kind of object, and
 * xpc_get_type() returns one of them.
 */
typedef const struct _xpc_type_s *xpc_type_t;

/*
 * extern, or each of these is a tentative definition of an incomplete
 * struct and the compiler rejects it: the type tags are objects living
 * in libSystem, and this only names them.
 */
#define XPC_TYPE(type)  extern const struct _xpc_type_s type

XPC_EXPORT XPC_TYPE(_xpc_type_connection);
#define XPC_TYPE_CONNECTION (&_xpc_type_connection)
XPC_EXPORT XPC_TYPE(_xpc_type_endpoint);
#define XPC_TYPE_ENDPOINT (&_xpc_type_endpoint)
XPC_EXPORT XPC_TYPE(_xpc_type_null);
#define XPC_TYPE_NULL (&_xpc_type_null)
XPC_EXPORT XPC_TYPE(_xpc_type_bool);
#define XPC_TYPE_BOOL (&_xpc_type_bool)
XPC_EXPORT XPC_TYPE(_xpc_type_int64);
#define XPC_TYPE_INT64 (&_xpc_type_int64)
XPC_EXPORT XPC_TYPE(_xpc_type_uint64);
#define XPC_TYPE_UINT64 (&_xpc_type_uint64)
XPC_EXPORT XPC_TYPE(_xpc_type_double);
#define XPC_TYPE_DOUBLE (&_xpc_type_double)
XPC_EXPORT XPC_TYPE(_xpc_type_date);
#define XPC_TYPE_DATE (&_xpc_type_date)
XPC_EXPORT XPC_TYPE(_xpc_type_data);
#define XPC_TYPE_DATA (&_xpc_type_data)
XPC_EXPORT XPC_TYPE(_xpc_type_string);
#define XPC_TYPE_STRING (&_xpc_type_string)
XPC_EXPORT XPC_TYPE(_xpc_type_uuid);
#define XPC_TYPE_UUID (&_xpc_type_uuid)
XPC_EXPORT XPC_TYPE(_xpc_type_fd);
#define XPC_TYPE_FD (&_xpc_type_fd)
XPC_EXPORT XPC_TYPE(_xpc_type_shmem);
#define XPC_TYPE_SHMEM (&_xpc_type_shmem)
XPC_EXPORT XPC_TYPE(_xpc_type_array);
#define XPC_TYPE_ARRAY (&_xpc_type_array)
XPC_EXPORT XPC_TYPE(_xpc_type_dictionary);
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
XPC_EXPORT XPC_TYPE(_xpc_type_error);
#define XPC_TYPE_ERROR (&_xpc_type_error)
XPC_EXPORT XPC_TYPE(_xpc_type_activity);
#define XPC_TYPE_ACTIVITY (&_xpc_type_activity)

/* The singletons.  There is one true and one false, and one null. */
XPC_EXPORT extern const struct _xpc_bool_s _xpc_bool_true;
#define XPC_BOOL_TRUE XPC_GLOBAL_OBJECT(_xpc_bool_true)
XPC_EXPORT extern const struct _xpc_bool_s _xpc_bool_false;
#define XPC_BOOL_FALSE XPC_GLOBAL_OBJECT(_xpc_bool_false)
XPC_EXPORT extern const struct _xpc_null_s _xpc_null;
#define XPC_NULL XPC_GLOBAL_OBJECT(_xpc_null)

/* Error dictionaries a connection hands back. */
XPC_EXPORT extern const struct _xpc_dictionary_s _xpc_error_connection_interrupted;
#define XPC_ERROR_CONNECTION_INTERRUPTED \
	XPC_GLOBAL_OBJECT(_xpc_error_connection_interrupted)
XPC_EXPORT extern const struct _xpc_dictionary_s _xpc_error_connection_invalid;
#define XPC_ERROR_CONNECTION_INVALID \
	XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
XPC_EXPORT extern const struct _xpc_dictionary_s _xpc_error_termination_imminent;
#define XPC_ERROR_TERMINATION_IMMINENT \
	XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)

XPC_EXPORT extern const char * const _xpc_error_key_description;
#define XPC_ERROR_KEY_DESCRIPTION _xpc_error_key_description

#ifdef __BLOCKS__
typedef void (^xpc_handler_t)(xpc_object_t object);
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t value);
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
#endif /* __BLOCKS__ */

/* --- lifetime and identity ------------------------------------- */

XPC_EXPORT XPC_NONNULL_ALL
xpc_object_t xpc_retain(xpc_object_t object);

XPC_EXPORT XPC_NONNULL_ALL
void xpc_release(xpc_object_t object);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_type_t xpc_get_type(xpc_object_t object);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const char *xpc_type_get_name(xpc_type_t type);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL_ALL
xpc_object_t xpc_copy(xpc_object_t object);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
bool xpc_equal(xpc_object_t object1, xpc_object_t object2);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_hash(xpc_object_t object);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_NONNULL_ALL
char *xpc_copy_description(xpc_object_t object);

/* --- the primitives -------------------------------------------- */

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_null_create(void);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_bool_create(bool value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
bool xpc_bool_get_value(xpc_object_t xbool);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_int64_create(int64_t value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_int64_get_value(xpc_object_t xint);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_uint64_create(uint64_t value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
uint64_t xpc_uint64_get_value(xpc_object_t xuint);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_double_create(double value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
double xpc_double_get_value(xpc_object_t xdouble);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_date_create(int64_t interval);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_date_create_from_current(void);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_date_get_value(xpc_object_t xdate);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_data_create(const void *_Nullable bytes, size_t length);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_data_create_with_dispatch_data(dispatch_data_t ddata);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_data_get_length(xpc_object_t xdata);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const void *_Nullable xpc_data_get_bytes_ptr(xpc_object_t xdata);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
size_t xpc_data_get_bytes(xpc_object_t xdata, void *buffer,
    size_t off, size_t length);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_string_create(const char *string);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1 XPC_PRINTF(1, 2)
xpc_object_t xpc_string_create_with_format(const char *fmt, ...);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_string_create_with_format_and_arguments(const char *fmt,
    va_list ap);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_string_get_length(xpc_object_t xstring);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const char *xpc_string_get_string_ptr(xpc_object_t xstring);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_uuid_create(const uuid_t uuid);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const uint8_t *xpc_uuid_get_bytes(xpc_object_t xuuid);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_fd_create(int fd);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int xpc_fd_dup(xpc_object_t xfd);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_shmem_create(void *region, size_t length);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_shmem_map(xpc_object_t xshmem, void *_Nullable *_Nonnull region);

/* --- arrays ----------------------------------------------------- */

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_array_create(xpc_object_t _Nonnull const *_Nullable objects,
    size_t count);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_array_create_empty(void);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_value(xpc_object_t xarray, size_t index, xpc_object_t value);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_array_get_count(xpc_object_t xarray);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t xpc_array_get_value(xpc_object_t xarray, size_t index);

#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL_ALL
bool xpc_array_apply(xpc_object_t xarray, XPC_NOESCAPE xpc_array_applier_t applier);
#endif

XPC_EXPORT XPC_NONNULL1
void xpc_array_set_bool(xpc_object_t xarray, size_t index, bool value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_int64(xpc_object_t xarray, size_t index, int64_t value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_uint64(xpc_object_t xarray, size_t index, uint64_t value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_double(xpc_object_t xarray, size_t index, double value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_date(xpc_object_t xarray, size_t index, int64_t value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_data(xpc_object_t xarray, size_t index,
    const void *bytes, size_t length);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_string(xpc_object_t xarray, size_t index, const char *string);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_uuid(xpc_object_t xarray, size_t index, const uuid_t uuid);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_fd(xpc_object_t xarray, size_t index, int fd);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_connection(xpc_object_t xarray, size_t index,
    xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
bool xpc_array_get_bool(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_array_get_int64(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
uint64_t xpc_array_get_uint64(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
double xpc_array_get_double(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_array_get_date(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const void *_Nullable xpc_array_get_data(xpc_object_t xarray, size_t index,
    size_t *length);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const char *_Nullable xpc_array_get_string(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const uint8_t *_Nullable xpc_array_get_uuid(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int xpc_array_dup_fd(xpc_object_t xarray, size_t index);

/* --- dictionaries ----------------------------------------------- */

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_dictionary_create(const char *_Nonnull const *_Nullable keys,
    xpc_object_t _Nullable const *_Nullable values, size_t count);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t xpc_dictionary_create_empty(void);

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED XPC_NONNULL1
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_value(xpc_object_t xdict, const char *key,
    xpc_object_t _Nullable value);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1 XPC_NONNULL2
xpc_object_t _Nullable xpc_dictionary_get_value(xpc_object_t xdict,
    const char *key);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_dictionary_get_count(xpc_object_t xdict);

#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL_ALL
bool xpc_dictionary_apply(xpc_object_t xdict,
    XPC_NOESCAPE xpc_dictionary_applier_t applier);
#endif

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t xdict);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_double(xpc_object_t xdict, const char *key, double value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_date(xpc_object_t xdict, const char *key, int64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_data(xpc_object_t xdict, const char *key,
    const void *bytes, size_t length);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_string(xpc_object_t xdict, const char *key,
    const char *string);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_uuid(xpc_object_t xdict, const char *key,
    const uuid_t uuid);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_fd(xpc_object_t xdict, const char *key, int fd);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_connection(xpc_object_t xdict, const char *key,
    xpc_connection_t connection);

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
bool xpc_dictionary_get_bool(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_dictionary_get_int64(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
uint64_t xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
double xpc_dictionary_get_double(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int64_t xpc_dictionary_get_date(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const void *_Nullable xpc_dictionary_get_data(xpc_object_t xdict,
    const char *key, size_t *_Nullable length);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const char *_Nullable xpc_dictionary_get_string(xpc_object_t xdict,
    const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
const uint8_t *_Nullable xpc_dictionary_get_uuid(xpc_object_t xdict,
    const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int xpc_dictionary_dup_fd(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t _Nullable xpc_dictionary_get_array(xpc_object_t xdict,
    const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t _Nullable xpc_dictionary_get_dictionary(xpc_object_t xdict,
    const char *key);

/* --- the service entry point ------------------------------------ */

#ifdef __BLOCKS__
/*
 * Hand the process over to XPC.  Does not return.
 */
XPC_EXPORT XPC_NORETURN XPC_NONNULL1
void xpc_main(xpc_connection_handler_t handler);
#endif

XPC_EXPORT XPC_WARN_RESULT XPC_MALLOC XPC_RETURNS_RETAINED
xpc_object_t _Nullable xpc_copy_bootstrap(void);

XPC_ASSUME_NONNULL_END
__END_DECLS

#include <xpc/endpoint.h>
#include <xpc/debug.h>
#include <xpc/connection.h>
#include <xpc/rich_error.h>
#include <xpc/session.h>
#include <xpc/listener.h>
#include <xpc/activity.h>
#include <xpc/peer_requirement.h>

#undef __XPC_INDIRECT__

#if __has_include(<launch.h>)
#include <launch.h>
#endif

#endif /* __XPC_H__ */
