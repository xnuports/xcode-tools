/*
 * cs_macho.c - Mach-O binary parsing.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "codesign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mach-o/loader.h>

static uint32_t
read32(const uint8_t *p, int is_le)
{
	if (is_le)
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	else
		return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t
read64(const uint8_t *p, int is_le)
{
	uint32_t lo = read32(p, is_le);
	uint32_t hi = read32(p + 4, is_le);
	return is_le ? ((uint64_t)lo | ((uint64_t)hi << 32))
	               : (((uint64_t)lo << 32) | (uint64_t)hi);
}

/* ---- Mach-O parsing ---- */

static int
parse_arch(const uint8_t *base, size_t size, struct arch_info *ai)
{
	uint32_t magic = be_read32(base);
	int is_64 = 0;
	int is_le = 1;
	uint32_t ncmds;
	size_t sizeofcmds_val, header_sz;

	(void)sizeofcmds_val;

	if (magic == MH_MAGIC || magic == MH_MAGIC_64)
		is_le = 0;  /* big-endian magic */
	else if (magic == MH_CIGAM || magic == MH_CIGAM_64)
		is_le = 1;  /* byte-swapped = little-endian file */
	else
		return -1;

	is_64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

	ai->base = base;
	ai->size = size;
	ai->is_64 = is_64;
	ai->is_le = is_le;
	ai->cputype = read32(base + 4, is_le);
	ai->cpusubtype = read32(base + 8, is_le);
	ai->file_type = read32(base + 12, is_le);
	ncmds = read32(base + 16, is_le);
	sizeofcmds_val = read32(base + 20, is_le);
	ai->flags = read32(base + 24, is_le);

	ai->code_limit = 0;
	ai->text_vmaddr = 0;
	ai->text_vmsize = 0;
	ai->text_fileoff = 0;
	ai->text_filesize = 0;
	ai->text_exec_flags = 0;
	ai->dataoff = 0;
	ai->datasize = 0;
	ai->linkedit_fileoff = 0;
	ai->linkedit_filesize = 0;

	header_sz = is_64 ? 32 : 28;
	size_t lc_off = header_sz;
	int found_codesig = 0;

	for (uint32_t i = 0; i < ncmds; i++) {
		if (lc_off + 8 > size)
			return -1;

		uint32_t cmd = read32(base + lc_off, is_le);
		uint32_t cmdsize = read32(base + lc_off + 4, is_le);
		if (cmdsize == 0)
			return -1;

		if (cmd == LC_CODE_SIGNATURE) {
			ai->dataoff = read32(base + lc_off + 8, is_le);
			ai->datasize = read32(base + lc_off + 12, is_le);
			found_codesig = 1;
		}

		if (cmd == LC_SEGMENT_64) {
			char segname[17];
			memset(segname, 0, sizeof(segname));
			memcpy(segname, base + lc_off + 8, 16);

			if (strcmp(segname, SEG_TEXT) == 0) {
				ai->text_vmaddr = read64(base + lc_off + 24, is_le);
				ai->text_vmsize = read64(base + lc_off + 32, is_le);
				ai->text_fileoff = read64(base + lc_off + 40, is_le);
				ai->text_filesize = read64(base + lc_off + 48, is_le);
				if (ai->file_type == MH_EXECUTE)
					ai->text_exec_flags |= CS_EXECSEG_MAIN_BINARY;
			} else if (strcmp(segname, "__LINKEDIT") == 0) {
				ai->linkedit_fileoff = read64(base + lc_off + 40, is_le);
				ai->linkedit_filesize = read64(base + lc_off + 48, is_le);
			}
		} else if (cmd == LC_SEGMENT) {
			char segname[17];
			memset(segname, 0, sizeof(segname));
			memcpy(segname, base + lc_off + 8, 16);

			if (strcmp(segname, SEG_TEXT) == 0) {
				ai->text_fileoff = read32(base + lc_off + 24, is_le);
				ai->text_filesize = read32(base + lc_off + 32, is_le);
				ai->text_vmaddr = read32(base + lc_off + 16, is_le);
				ai->text_vmsize = read32(base + lc_off + 20, is_le);
				if (ai->file_type == MH_EXECUTE)
					ai->text_exec_flags |= CS_EXECSEG_MAIN_BINARY;
			}
		}

		lc_off += cmdsize;
	}

	/* Read exec_seg_flags from existing CodeDirectory */
	if (found_codesig && ai->dataoff + 12 <= size) {
		const uint8_t *cs = base + ai->dataoff;
		uint32_t sb_magic = be_read32(cs);
		if (sb_magic == CSMAGIC_EMBEDDED_SIGNATURE) {
			uint32_t sb_count = be_read32(cs + 8);
			size_t idx_off = ai->dataoff + 12;
			for (uint32_t k = 0; k < sb_count; k++) {
				if (idx_off + 8 > size)
					break;
				uint32_t slot_type = be_read32(base + idx_off);
				uint32_t slot_off = be_read32(base + idx_off + 4);
				if (slot_type == CSSLOT_CODEDIRECTORY &&
				    ai->dataoff + slot_off + 88 <= size) {
					const uint8_t *cd = base + ai->dataoff + slot_off;
					uint32_t cd_ver = be_read32(cd + 8);
					if (cd_ver >= 0x20400)
						ai->text_exec_flags = be_read64(cd + 80);
					break;
				}
				idx_off += 8;
			}
		}
	}

	/* code_limit */
	if (found_codesig)
		ai->code_limit = ai->dataoff;
	else
		ai->code_limit = size;

	return 0;
}

int
macho_parse(const uint8_t *data, size_t size, struct macho_file *mf)
{
	uint32_t magic = be_read32(data);

	if (magic == MH_MAGIC || magic == MH_MAGIC_64 ||
	    magic == MH_CIGAM || magic == MH_CIGAM_64) {
		mf->is_fat = 0;
		mf->n_archs = 1;
		mf->data = data;
		mf->size = size;
		if (parse_arch(data, size, &mf->archs[0]) != 0)
			return -1;
		return 0;
	}

	if (magic == 0xcafebabe || magic == 0xbebafeca) {
		int is_fat_le = (magic == 0xbebafeca);
		mf->is_fat = 1;
		mf->data = data;
		mf->size = size;
		mf->n_archs = read32(data + 4, is_fat_le);
		if (mf->n_archs > 16 || mf->n_archs == 0)
			return -1;

		size_t arch_off = 8;
		for (uint32_t i = 0; i < mf->n_archs; i++) {
			if (arch_off + 20 > size)
				return -1;
			uint32_t off = read32(data + arch_off + 8, is_fat_le);
			uint32_t sz = read32(data + arch_off + 12, is_fat_le);
			arch_off += 20;

			mf->archs[i].cputype =
			    read32(data + arch_off - 20, is_fat_le);
			mf->archs[i].cpusubtype =
			    read32(data + arch_off - 20 + 4, is_fat_le);

			if (off + sz > size)
				return -1;

			if (parse_arch(data + off, sz, &mf->archs[i]) != 0)
				return -1;
		}
		return 0;
	}

	return -1;
}

int
macho_has_codesig(struct arch_info *ai)
{
	return (ai->dataoff > 0 && ai->datasize > 0);
}

const char *
macho_arch_name(uint32_t cputype)
{
	switch (cputype) {
	case 0x01000007: return "arm64";
	case 0x01000008: return "arm64e";
	case 0x01000003: return "x86_64";
	case 0x00000007: return "i386";
	case 0x02000000: return "powerpc";
	case 0x02000008: return "powerpc64";
	default:         return NULL;
	}
}

int
macho_is_le(struct arch_info *ai)
{
	return ai->is_le;
}

uint32_t
macho_file_type(struct arch_info *ai)
{
	return ai->file_type;
}

uint32_t
macho_cputype(struct arch_info *ai)
{
	return ai->cputype;
}

uint64_t
macho_code_limit(struct arch_info *ai)
{
	return ai->code_limit;
}

uint32_t
macho_dataoff(struct arch_info *ai)
{
	return ai->dataoff;
}

uint32_t
macho_datasize(struct arch_info *ai)
{
	return ai->datasize;
}

uint64_t
macho_exec_seg_flags(struct arch_info *ai)
{
	return ai->text_exec_flags;
}

uint32_t
macho_header_size(struct arch_info *ai)
{
	return ai->is_64 ? 32 : 28;
}

/*
 * Find the offset of LC_CODE_SIGNATURE in a Mach-O.
 * Returns -1 if not found.
 */
long
macho_find_codesig_lc(struct arch_info *ai)
{
	size_t header_sz = ai->is_64 ? 32 : 28;
	uint32_t magic = be_read32(ai->base);
	int is_le = (magic == MH_CIGAM || magic == MH_CIGAM_64);
	uint32_t ncmds = read32(ai->base + 16, is_le);

	size_t lc_off = header_sz;
	for (uint32_t i = 0; i < ncmds; i++) {
		uint32_t cmd = read32(ai->base + lc_off, is_le);
		if (cmd == LC_CODE_SIGNATURE)
			return (long)lc_off;
		lc_off += read32(ai->base + lc_off + 4, is_le);
	}
	return -1;
}

/*
 * Update the LC_CODE_SIGNATURE dataoff and datasize.
 * If no LC exists, adds one to the load command area.
 */
int
macho_update_codesig_lc(struct arch_info *ai, uint32_t new_dataoff,
    uint32_t new_datasize, uint8_t *file_buf, size_t file_sz)
{
	long lc_off = macho_find_codesig_lc(ai);
	int is_64 = ai->is_64;
	int is_le = ai->is_le;

	if (lc_off >= 0) {
		/* Update existing LC */
		if (is_le) {
			uint32_t doff = new_dataoff;
			uint32_t dsize = new_datasize;
			memcpy((uint8_t *)ai->base + lc_off + 8, &doff, 4);
			memcpy((uint8_t *)ai->base + lc_off + 12, &dsize, 4);
		} else {
			/* Big-endian - write manually */
			be_write32((uint8_t *)ai->base + lc_off + 8, new_dataoff);
			be_write32((uint8_t *)ai->base + lc_off + 12, new_datasize);
		}
	} else {
		/* Add new LC_CODE_SIGNATURE */
		size_t header_sz = is_64 ? 32 : 28;
		uint32_t ncmds = read32(ai->base + 16, is_le);
		uint32_t sizeofcmds = read32(ai->base + 20, is_le);

		/* Need space: LC is 16 bytes */
		if (sizeofcmds + 16 > SIZE_MAX - header_sz)
			return -1;

		size_t available = (size_t)sizeofcmds + header_sz;
		if (available + 16 > file_sz)
			return -1;

		/* Write the new LC at the end of existing load commands */
		size_t new_lc_off = header_sz + sizeofcmds;

		/* Write cmd and cmdsize in correct endianness */
		if (is_le) {
			uint32_t cmd_val = LC_CODE_SIGNATURE;
			uint32_t cmdsz = 16;
			memcpy((uint8_t *)ai->base + new_lc_off, &cmd_val, 4);
			memcpy((uint8_t *)ai->base + new_lc_off + 4, &cmdsz, 4);
			memcpy((uint8_t *)ai->base + new_lc_off + 8, &new_dataoff, 4);
			memcpy((uint8_t *)ai->base + new_lc_off + 12, &new_datasize, 4);
		} else {
			be_write32((uint8_t *)ai->base + new_lc_off, LC_CODE_SIGNATURE);
			be_write32((uint8_t *)ai->base + new_lc_off + 4, 16);
			be_write32((uint8_t *)ai->base + new_lc_off + 8, new_dataoff);
			be_write32((uint8_t *)ai->base + new_lc_off + 12, new_datasize);
		}

		/* Update ncmds and sizeofcmds in the header */
		if (is_le) {
			uint32_t new_ncmds = ncmds + 1;
			uint32_t new_sizeofcmds = sizeofcmds + 16;
			memcpy((uint8_t *)ai->base + 16, &new_ncmds, 4);
			memcpy((uint8_t *)ai->base + 20, &new_sizeofcmds, 4);
		} else {
			be_write32((uint8_t *)ai->base + 16, ncmds + 1);
			be_write32((uint8_t *)ai->base + 20, sizeofcmds + 16);
		}
	}

	return 0;
}

/*
 * Update the __LINKEDIT segment's filesize (and vmsize if needed)
 * so that it covers the newly written signature data.
 */
int
macho_update_linkedit_seg(struct arch_info *ai, uint32_t sig_dataoff,
    uint32_t sig_datasize)
{
	if (ai->linkedit_fileoff == 0)
		return -1;

	uint64_t new_end = (uint64_t)sig_dataoff + sig_datasize;
	uint64_t new_filesize = new_end - ai->linkedit_fileoff;

	ai->linkedit_filesize = new_filesize;

	uint64_t vm_page = 4096;
	uint64_t new_vmsize = (new_filesize + vm_page - 1) & ~(vm_page - 1);

	size_t header_sz = ai->is_64 ? 32 : 28;
	uint32_t ncmds = read32(ai->base + 16, ai->is_le);

	size_t lc_off = header_sz;
	for (uint32_t i = 0; i < ncmds; i++) {
		uint32_t cmd = read32(ai->base + lc_off, ai->is_le);
		uint32_t cmdsize = read32(ai->base + lc_off + 4, ai->is_le);

		if (cmd == LC_SEGMENT_64) {
			char segname[17];
			memset(segname, 0, sizeof(segname));
			memcpy(segname, (uint8_t *)ai->base + lc_off + 8, 16);

			if (strcmp(segname, "__LINKEDIT") == 0) {
				if (ai->is_le) {
					memcpy((uint8_t *)ai->base + lc_off + 32, &new_vmsize, 8);
					memcpy((uint8_t *)ai->base + lc_off + 48, &ai->linkedit_filesize, 8);
				} else {
					be_write64((uint8_t *)ai->base + lc_off + 32, new_vmsize);
					be_write64((uint8_t *)ai->base + lc_off + 48, ai->linkedit_filesize);
				}
				return 0;
			}
		}
		lc_off += cmdsize;
	}

	return -1;
}
