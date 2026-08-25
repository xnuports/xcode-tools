/*
 * mach-o/cctools_helpers.h
 *
 * Apple's cctools sources include this header, but neither it nor the
 * libcctoolshelper library that implements it appears in the cctools
 * open-source release or in a shipped Xcode -- they are build-time-only
 * Apple internals, statically linked into Apple's binaries.
 *
 * This is our own reimplementation of the interface, declared here so
 * that cctools' misc/libtool.c compiles unmodified.  The submodule is
 * never edited; only this header and its implementation in
 * src/cctools-helpers/ are ours.
 *
 * Copyright (c) 2026 Sunneva N. Mariu
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _MACH_O_CCTOOLS_HELPERS_H_
#define _MACH_O_CCTOOLS_HELPERS_H_

#include <mach/machine.h>
#include <stdint.h>

/*
 * make_obj_file_with_linker_options() writes a temporary MH_OBJECT file
 * for the given architecture whose only content is a series of
 * LC_LINKER_OPTION load commands, one per requested library and
 * framework.  libtool(1) adds the result to the archive it is building
 * so that anything later linking against that archive automatically
 * pulls in the referenced libraries -- this backs libtool's -ref-l and
 * -ref-framework options.
 *
 * The absolute path of the created file is written to obj_path, which
 * must point to a buffer of at least PATH_MAX bytes.  The caller owns
 * the file and is expected to unlink() it, as libtool does.
 *
 * reflibs holds bare library names ("foo" for -lfoo) and reffw bare
 * framework names, matching how libtool.c collects them.
 */
extern void make_obj_file_with_linker_options(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    uint32_t nreflibs,
    const char **reflibs,
    uint32_t nreffw,
    const char **reffw,
    char *obj_path);

#endif /* _MACH_O_CCTOOLS_HELPERS_H_ */
