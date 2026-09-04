/*
 * MacTypes.h -- the base scalar types the Darwin APIs are written in.
 *
 * OSStatus, Boolean, the sized integer names and the handful of others
 * that CoreFoundation and everything descended from Carbon spell their
 * interfaces with.  Apple's SDK ships this header and no open-source
 * release in this tree carries it: it belongs to CarbonHeaders, which
 * Apple does not publish.
 *
 * What is here is the base type layer, not the whole of Apple's header.
 * Every definition is fixed by the ABI -- OSStatus is a signed 32-bit
 * integer, Boolean is an unsigned char -- so a program built against
 * this lays out identically to one built against Apple's.  The Carbon
 * era's higher-level declarations are deliberately absent; nothing in
 * this tree uses them, and guessing at them would be worse than
 * leaving them out.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MACTYPES__
#define __MACTYPES__

/*
 * Apple's copy includes <ConditionalMacros.h> here.  That header is
 * CarbonHeaders' too and is not in this tree either; nothing below
 * needs it, so it is not included rather than stubbed.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The sized scalars. */
typedef unsigned char           UInt8;
typedef signed char             SInt8;
typedef unsigned short          UInt16;
typedef signed short            SInt16;
typedef unsigned int            UInt32;
typedef signed int              SInt32;
typedef long long               SInt64;
typedef unsigned long long      UInt64;

typedef float                   Float32;
typedef double                  Float64;

/* Error and status returns. */
typedef SInt16                  OSErr;
typedef SInt32                  OSStatus;

/*
 * Boolean is an unsigned char here and not C's _Bool: it is an ABI type
 * that predates <stdbool.h>, and something compiled against a one-byte
 * unsigned char has to keep seeing one.
 */
typedef unsigned char           Boolean;

/* Four-character codes. */
typedef UInt32                  FourCharCode;
typedef FourCharCode            OSType;
typedef FourCharCode            ResType;
typedef OSType *                OSTypePtr;
typedef ResType *               ResTypePtr;

/* Addresses and raw memory. */
typedef char *                  Ptr;
typedef Ptr *                   Handle;
typedef long                    Size;
typedef void *                  LogicalAddress;
typedef const void *            ConstLogicalAddress;
typedef void *                  PhysicalAddress;
typedef UInt8 *                 BytePtr;
typedef unsigned long           ByteCount;
typedef unsigned long           ByteOffset;
typedef unsigned long           ItemCount;

/* Bit flags and time. */
typedef UInt32                  OptionBits;
typedef SInt32                  Duration;
typedef UInt64                  AbsoluteTime;

/* Text. */
typedef UInt16                  UniChar;
typedef UniChar *               UniCharPtr;
typedef unsigned long           UniCharCount;
typedef UniCharCount *          UniCharCountPtr;
typedef unsigned char           Str255[256];
typedef unsigned char           Str63[64];
typedef unsigned char           Str32[33];
typedef unsigned char           Str31[32];
typedef unsigned char *         StringPtr;
typedef const unsigned char *   ConstStringPtr;
typedef StringPtr *             StringHandle;

/* The classic no-error value. */
enum {
    noErr                       = 0
};

#ifdef __cplusplus
}
#endif


/*
 * CoreFoundation's public headers reach for these: CFString.h takes
 * ConstStr255Param and the UTF character types, CFCharacterSet.h takes
 * UTF32Char, and CFLocale.h still carries the classic LangCode and
 * RegionCode in its deprecated conversion calls.
 */
typedef UInt16                          UTF16Char;
typedef UInt8                           UTF8Char;
typedef UInt32                          UTF32Char;

typedef const unsigned char *           ConstStr255Param;
typedef const unsigned char *           ConstStringPtr;

typedef SInt16                          LangCode;
typedef SInt16                          RegionCode;

#endif /* __MACTYPES__ */