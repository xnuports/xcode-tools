/*
 * ConditionalMacros.h -- the Carbon-era compatibility macros.
 *
 * This header belongs to CarbonCore, which is not open source, and
 * Apple ships it in usr/include.  Security's cssmconfig.h includes it
 * and then tests TARGET_OS_MAC, so the SDK needs it.
 *
 * Apple's own copy has been a compatibility shim for a long time: the
 * TARGET_* macros it used to define now come from TargetConditionals.h,
 * and what is left is the PRAGMA_*, TYPE_* and FUNCTION_* set that
 * Carbon-era headers still name.  That is what this provides -- the
 * modern values, where every one of those questions has a settled
 * answer for clang on Darwin.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __CONDITIONALMACROS__
#define __CONDITIONALMACROS__

#include <TargetConditionals.h>

/*
 * Pragma support.  clang has #pragma once and the struct-packing
 * pragmas; it has never had the CodeWarrior enum pragmas.
 */
#ifndef PRAGMA_ONCE
#define PRAGMA_ONCE                 1
#endif
#ifndef PRAGMA_IMPORT
#define PRAGMA_IMPORT               0
#endif
#ifndef PRAGMA_STRUCT_ALIGN
#define PRAGMA_STRUCT_ALIGN         0
#endif
#ifndef PRAGMA_STRUCT_PACK
#define PRAGMA_STRUCT_PACK          1
#endif
#ifndef PRAGMA_STRUCT_PACKPUSH
#define PRAGMA_STRUCT_PACKPUSH      1
#endif
#ifndef PRAGMA_ENUM_PACK
#define PRAGMA_ENUM_PACK            0
#endif
#ifndef PRAGMA_ENUM_ALWAYSINT
#define PRAGMA_ENUM_ALWAYSINT       0
#endif
#ifndef PRAGMA_ENUM_OPTIONS
#define PRAGMA_ENUM_OPTIONS         0
#endif

/* Language features. */
#ifndef TYPE_EXTENDED
#define TYPE_EXTENDED               0
#endif
#ifndef TYPE_LONGLONG
#define TYPE_LONGLONG               1
#endif
#ifndef TYPE_BOOL
#ifdef __cplusplus
#define TYPE_BOOL                   1
#else
#define TYPE_BOOL                   0
#endif
#endif

/*
 * Calling conventions.  Pascal calling and the Win32 conventions are
 * long gone; what remains is the plain C one.
 */
#ifndef FUNCTION_PASCAL
#define FUNCTION_PASCAL             0
#endif
#ifndef FUNCTION_DECLSPEC
#define FUNCTION_DECLSPEC           0
#endif
#ifndef FUNCTION_WIN32CC
#define FUNCTION_WIN32CC            0
#endif

/*
 * The EXTERN_API family, which is how Carbon headers spell an
 * external declaration.  With Pascal calling gone they are all the
 * same thing.
 */
#ifdef __cplusplus
#define EXTERN_API(_type)           extern "C" _type
#define EXTERN_API_C(_type)         extern "C" _type
#define EXTERN_API_STDCALL(_type)   extern "C" _type
#define EXTERN_API_C_STDCALL(_type) extern "C" _type
#else
#define EXTERN_API(_type)           extern _type
#define EXTERN_API_C(_type)         extern _type
#define EXTERN_API_STDCALL(_type)   extern _type
#define EXTERN_API_C_STDCALL(_type) extern _type
#endif

#define DEFINE_API(_type)           _type
#define DEFINE_API_C(_type)         _type
#define DEFINE_API_STDCALL(_type)   _type
#define DEFINE_API_C_STDCALL(_type) _type

#define CALLBACK_API(_type, _name)          _type (*_name)
#define CALLBACK_API_C(_type, _name)        _type (*_name)
#define CALLBACK_API_STDCALL(_type, _name)  _type (*_name)

/*
 * FOUR_CHAR_CODE, which Carbon headers use to write an OSType as a
 * character literal.
 */
#ifndef FOUR_CHAR_CODE
#define FOUR_CHAR_CODE(x)           (x)
#endif

#endif /* __CONDITIONALMACROS__ */
