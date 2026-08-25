/*
 * cctools_helpers.c
 *
 * Our reimplementation of the one function cctools' libtool(1) needs
 * from Apple's libcctoolshelper, which ships in neither the cctools
 * open-source release nor Xcode.  See include/mach-o/cctools_helpers.h.
 *
 * Copyright (c) 2026 Sunneva N. Mariu
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mach/vm_prot.h>
#include <mach-o/loader.h>

#include "stuff/arch.h"
#include "stuff/bytesex.h"
#include "stuff/errors.h"

#include <mach-o/cctools_helpers.h>

/*
 * A load command's size must be a multiple of 4 in a 32-bit object and
 * a multiple of 8 in a 64-bit one (mach-o/loader.h).
 */
static uint32_t
round_up(uint32_t value, uint32_t multiple)
{
	return ((value + multiple - 1) / multiple) * multiple;
}

/*
 * Byte-order helpers.  The buffer is assembled in target byte order so
 * the result is correct even when cross-building for an architecture
 * whose byte sex differs from the host's.
 */
static void
put32(uint8_t *p, uint32_t v, enum byte_sex target)
{
	if(target == BIG_ENDIAN_BYTE_SEX){
		p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
		p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)(v);
	}
	else{
		p[0] = (uint8_t)(v);       p[1] = (uint8_t)(v >>  8);
		p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
	}
}

static void
put64(uint8_t *p, uint64_t v, enum byte_sex target)
{
	if(target == BIG_ENDIAN_BYTE_SEX){
		put32(p,     (uint32_t)(v >> 32), target);
		put32(p + 4, (uint32_t)(v),       target);
	}
	else{
		put32(p,     (uint32_t)(v),       target);
		put32(p + 4, (uint32_t)(v >> 32), target);
	}
}

/*
 * One LC_LINKER_OPTION carries a count followed by that many
 * NUL-terminated strings, zero-filled to the command's alignment.  ld64
 * recognises exactly two shapes (src/ld/Resolver.cpp): a single string
 * "-lNAME", or the pair "-framework", "NAME".  Anything else draws
 * "unknown linker option from object file ignored", so those are the
 * only two we emit.
 */
static uint32_t
linker_option_size(uint32_t nstrings, const char **strings, uint32_t align)
{
	uint32_t size, i;

	size = (uint32_t)sizeof(struct linker_option_command);
	for(i = 0; i < nstrings; i++)
		size += (uint32_t)strlen(strings[i]) + 1;
	return(round_up(size, align));
}

static uint8_t *
emit_linker_option(uint8_t *p, uint32_t nstrings, const char **strings,
		   uint32_t align, enum byte_sex target)
{
	uint32_t cmdsize, i, len;
	uint8_t *start;

	start = p;
	cmdsize = linker_option_size(nstrings, strings, align);

	put32(p, LC_LINKER_OPTION, target); p += 4;
	put32(p, cmdsize, target);          p += 4;
	put32(p, nstrings, target);         p += 4;

	for(i = 0; i < nstrings; i++){
		len = (uint32_t)strlen(strings[i]) + 1;
		memcpy(p, strings[i], len);
		p += len;
	}

	/* zero fill to the command's aligned size */
	memset(p, 0, (size_t)(cmdsize - (uint32_t)(p - start)));

	return(start + cmdsize);
}

void
make_obj_file_with_linker_options(
cpu_type_t cputype,
cpu_subtype_t cpusubtype,
uint32_t nreflibs,
const char **reflibs,
uint32_t nreffw,
const char **reffw,
char *obj_path)
{
	struct arch_flag arch_flag;
	enum byte_sex target;
	enum bool is64;
	uint32_t align, header_size, sizeofcmds, ncmds, total, seg_size, i;
	uint8_t *buf, *p;
	char *tmpdir, **libopt, **fwopt;
	int fd;

	/*
	 * arm64_32 sets CPU_ARCH_ABI64_32, not CPU_ARCH_ABI64, and uses
	 * the 32-bit header -- so test for ABI64 specifically.
	 */
	is64 = (enum bool)((cputype & CPU_ARCH_ABI64) == CPU_ARCH_ABI64);
	align = is64 ? 8 : 4;
	header_size = is64 ? (uint32_t)sizeof(struct mach_header_64)
			   : (uint32_t)sizeof(struct mach_header);

	arch_flag.name = NULL;
	arch_flag.cputype = cputype;
	arch_flag.cpusubtype = cpusubtype;
	target = get_byte_sex_from_flag(&arch_flag);
	if(target == UNKNOWN_BYTE_SEX)
		target = get_host_byte_sex();

	/*
	 * Build the option string arrays: "-lNAME" for each library, and
	 * the "-framework"/NAME pair for each framework.
	 */
	libopt = NULL;
	if(nreflibs != 0){
		libopt = calloc(nreflibs, sizeof(char *));
		if(libopt == NULL)
			system_fatal("virtual memory exhausted (calloc failed)");
		for(i = 0; i < nreflibs; i++){
			size_t n = strlen(reflibs[i]) + 3;
			libopt[i] = malloc(n);
			if(libopt[i] == NULL)
				system_fatal("virtual memory exhausted (malloc "
					     "failed)");
			snprintf(libopt[i], n, "-l%s", reflibs[i]);
		}
	}
	fwopt = NULL;
	if(nreffw != 0){
		fwopt = calloc(nreffw, sizeof(char *));
		if(fwopt == NULL)
			system_fatal("virtual memory exhausted (calloc failed)");
		for(i = 0; i < nreffw; i++)
			fwopt[i] = (char *)reffw[i];
	}

	/*
	 * An MH_OBJECT must carry at least one segment load command even
	 * when it has no sections: ld64 rejects one that does not with
	 * "missing LC_SEGMENT file ... for architecture".  Real compilers
	 * emit a single unnamed segment here, so we do the same.
	 */
	seg_size = is64 ? (uint32_t)sizeof(struct segment_command_64)
			: (uint32_t)sizeof(struct segment_command);

	ncmds = 1 + nreflibs + nreffw;
	sizeofcmds = seg_size;
	for(i = 0; i < nreflibs; i++)
		sizeofcmds += linker_option_size(1, (const char **)&libopt[i],
						 align);
	for(i = 0; i < nreffw; i++){
		const char *pair[2];
		pair[0] = "-framework";
		pair[1] = fwopt[i];
		sizeofcmds += linker_option_size(2, pair, align);
	}

	total = header_size + sizeofcmds;
	buf = calloc(1, total);
	if(buf == NULL)
		system_fatal("virtual memory exhausted (calloc failed)");

	/*
	 * mach_header and mach_header_64 share their first seven 32-bit
	 * fields; the 64-bit form adds a reserved word.  calloc() has
	 * already zeroed the reserved field and the flags.
	 */
	p = buf;
	put32(p, is64 ? MH_MAGIC_64 : MH_MAGIC, target);   p += 4;
	put32(p, (uint32_t)cputype, target);               p += 4;
	put32(p, (uint32_t)cpusubtype, target);            p += 4;
	put32(p, MH_OBJECT, target);                       p += 4;
	put32(p, ncmds, target);                           p += 4;
	put32(p, sizeofcmds, target);                      p += 4;
	put32(p, 0, target);                               p += 4; /* flags */

	/*
	 * The empty segment.  segname is left as 16 zero bytes, which is
	 * what a compiler emits for an object file's single segment; the
	 * file offset points past the load commands, with zero length.
	 */
	p = buf + header_size;
	put32(p, is64 ? LC_SEGMENT_64 : LC_SEGMENT, target); p += 4;
	put32(p, seg_size, target);                          p += 4;
	memset(p, 0, 16);                                    p += 16; /* segname */
	if(is64){
		put64(p, 0, target); p += 8;            /* vmaddr   */
		put64(p, 0, target); p += 8;            /* vmsize   */
		put64(p, header_size + sizeofcmds, target); p += 8; /* fileoff */
		put64(p, 0, target); p += 8;            /* filesize */
	}
	else{
		put32(p, 0, target); p += 4;
		put32(p, 0, target); p += 4;
		put32(p, header_size + sizeofcmds, target); p += 4;
		put32(p, 0, target); p += 4;
	}
	put32(p, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE, target); p += 4;
	put32(p, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE, target); p += 4;
	put32(p, 0, target); p += 4;                    /* nsects */
	put32(p, 0, target); p += 4;                    /* flags  */

	for(i = 0; i < nreflibs; i++)
		p = emit_linker_option(p, 1, (const char **)&libopt[i], align,
				       target);
	for(i = 0; i < nreffw; i++){
		const char *pair[2];
		pair[0] = "-framework";
		pair[1] = fwopt[i];
		p = emit_linker_option(p, 2, pair, align, target);
	}

	tmpdir = getenv("TMPDIR");
	if(tmpdir == NULL || *tmpdir == '\0')
		tmpdir = "/tmp";
	snprintf(obj_path, PATH_MAX, "%s/libtool-linker-options.XXXXXX",
		 tmpdir);
	fd = mkstemp(obj_path);
	if(fd == -1)
		system_fatal("can't create temporary file: %s", obj_path);
	if(write(fd, buf, total) != (ssize_t)total)
		system_fatal("can't write temporary file: %s", obj_path);
	if(close(fd) == -1)
		system_fatal("can't close temporary file: %s", obj_path);

	for(i = 0; i < nreflibs; i++)
		free(libopt[i]);
	free(libopt);
	free(fwopt);
	free(buf);
}
