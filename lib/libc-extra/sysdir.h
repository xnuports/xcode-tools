/*
 * sysdir.h -- enumerate the standard system search paths.
 *
 * The public interface to sysdir_start_search_path_enumeration and
 * sysdir_get_next_search_path_enumeration, the C API Foundation's
 * FileManager search-path lookups are built on.  The implementation
 * lives in the system's libSystem and is reached through its stub at
 * link time; this only declares the interface.
 *
 * The enumerator values and the domain-mask bits are the platform ABI
 * and match Apple's; the two functions' signatures are their public
 * prototypes.  See the README beside this file for why the header is
 * here rather than in lib/libc.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __SYSTEM_DIRECTORIES_H__
#define __SYSTEM_DIRECTORIES_H__

#include <os/base.h>
#include <Availability.h>

/*
 * Which directory to look for.  Not every value names a directory that
 * exists on every platform; one that does not simply enumerates to no
 * path.  Available since macOS 10.12.
 */
OS_ENUM(sysdir_search_path_directory, unsigned int,
    SYSDIR_DIRECTORY_APPLICATION            = 1,    /* Applications */
    SYSDIR_DIRECTORY_DEMO_APPLICATION       = 2,    /* Applications/Demos */
    SYSDIR_DIRECTORY_DEVELOPER_APPLICATION  = 3,    /* Developer/Applications (soft-deprecated) */
    SYSDIR_DIRECTORY_ADMIN_APPLICATION      = 4,    /* Applications/Utilities */
    SYSDIR_DIRECTORY_LIBRARY                = 5,    /* Library */
    SYSDIR_DIRECTORY_DEVELOPER              = 6,    /* Developer (soft-deprecated) */
    SYSDIR_DIRECTORY_USER                   = 7,    /* Users */
    SYSDIR_DIRECTORY_DOCUMENTATION          = 8,    /* Library/Documentation */
    SYSDIR_DIRECTORY_DOCUMENT               = 9,    /* Documents */
    SYSDIR_DIRECTORY_CORESERVICE            = 10,   /* Library/CoreServices */
    SYSDIR_DIRECTORY_AUTOSAVED_INFORMATION  = 11,   /* Library/Autosave Information */
    SYSDIR_DIRECTORY_DESKTOP                = 12,   /* Desktop */
    SYSDIR_DIRECTORY_CACHES                 = 13,   /* Library/Caches */
    SYSDIR_DIRECTORY_APPLICATION_SUPPORT    = 14,   /* Library/Application Support */
    SYSDIR_DIRECTORY_DOWNLOADS              = 15,   /* Downloads */
    SYSDIR_DIRECTORY_INPUT_METHODS          = 16,   /* Library/Input Methods */
    SYSDIR_DIRECTORY_MOVIES                 = 17,   /* Movies */
    SYSDIR_DIRECTORY_MUSIC                  = 18,   /* Music */
    SYSDIR_DIRECTORY_PICTURES               = 19,   /* Pictures */
    SYSDIR_DIRECTORY_PRINTER_DESCRIPTION    = 20,   /* Library/Printers/PPDs */
    SYSDIR_DIRECTORY_SHARED_PUBLIC          = 21,   /* Public */
    SYSDIR_DIRECTORY_PREFERENCE_PANES       = 22,   /* Library/PreferencePanes */
    SYSDIR_DIRECTORY_ALL_APPLICATIONS       = 100,  /* every place an application can live */
    SYSDIR_DIRECTORY_ALL_LIBRARIES          = 101,  /* every place a resource can live */
);

/*
 * Which domains to search, as a bit mask.  Available since macOS 10.12.
 */
OS_OPTIONS(sysdir_search_path_domain_mask, unsigned int,
    SYSDIR_DOMAIN_MASK_USER                 = ( 1UL << 0 ), /* ~ */
    SYSDIR_DOMAIN_MASK_LOCAL                = ( 1UL << 1 ), /* this machine, all users */
    SYSDIR_DOMAIN_MASK_NETWORK              = ( 1UL << 2 ), /* /Network */
    SYSDIR_DOMAIN_MASK_SYSTEM               = ( 1UL << 3 ), /* provided by the OS */
    SYSDIR_DOMAIN_MASK_ALL                  = 0x0ffff,      /* every domain */
);

/*
 * An opaque enumeration cursor.  Zero means the enumeration is done.
 */
typedef unsigned int sysdir_search_path_enumeration_state;

__BEGIN_DECLS

/*
 * Begin enumerating the paths for a directory in the given domains, and
 * return the first cursor.  Feed each cursor to the call below.
 */
extern sysdir_search_path_enumeration_state
sysdir_start_search_path_enumeration(sysdir_search_path_directory_t dir,
    sysdir_search_path_domain_mask_t domainMask)
    __API_AVAILABLE(macosx(10.12), ios(10.0), watchos(3.0), tvos(10.0));

/*
 * Write the next path (PATH_MAX bytes) into path and return the cursor
 * for the call after; a returned cursor of zero means there are no more.
 */
extern sysdir_search_path_enumeration_state
sysdir_get_next_search_path_enumeration(sysdir_search_path_enumeration_state state,
    char *path)
    __API_AVAILABLE(macosx(10.12), ios(10.0), watchos(3.0), tvos(10.0));

__END_DECLS

#endif /* defined(__SYSTEM_DIRECTORIES_H__) */
