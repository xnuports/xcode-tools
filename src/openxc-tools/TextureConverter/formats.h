/*
 * formats.h -- the pixel formats a Khronos container can name.
 *
 * KTX version 1 identifies its format with an OpenGL internal enumerant and
 * version 2 with a Vulkan one, and TextureConverter prints neither: it
 * prints its own name for the format, which is what this maps to.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_FORMATS_H
#define TEXTURECONVERTER_FORMATS_H

#include <stdint.h>

/* NULL when the enumerant is one this tool has no name for. */
const char *format_name_for_gl(uint32_t gl_internal_format);
const char *format_name_for_vk(uint32_t vk_format);

/* True for a format whose samples are floating point, which is what makes
 * the colour space extended-range linear rather than plain sRGB. */
_Bool	format_is_float(const char *name);

/*
 * The OpenGL internal format a name compresses to, the base internal format
 * recorded beside it, the block it packs pixels into, and the MTLPixelFormat
 * a Khronos container carries (zero for a format Apple record none for).
 * Returns false for a name this tool does not write.
 *
 * The base format is not derivable from the internal one: it says how many
 * channels the format actually carries, so BC4 and EAC_R11 are GL_RED, BC5
 * and EAC_RG11 GL_RG, BC6 and ETC2_RGB8 GL_RGB, and the rest GL_RGBA.
 */
_Bool	format_lookup(const char *name, uint32_t *gl, uint32_t *base,
	    int *block_x, int *block_y, uint32_t *metal);

/*
 * What a KTX2 data format descriptor has to say about a format: which
 * colour model the blocks belong to, and one or two samples describing the
 * bits.  Everything else in the descriptor is either constant or derivable
 * from the block size, so only this is tabulated.
 */
struct format_sample {
	uint16_t	bit_offset;
	uint8_t		bit_length;	/* one less than the count, as KTX2 */
	uint8_t		channel_type;
	uint32_t	lower, upper;
};

struct format_dfd {
	uint8_t			color_model;
	int			nsamples;
	struct format_sample	sample[4];	/* one per channel, at most */
};

/* The Vulkan enumerant a name is written as, or zero. */
uint32_t format_vk_for(const char *name);

/* False only for a name this tool does not know at all. */
_Bool	format_dfd_for(const char *name, struct format_dfd *);

#endif /* TEXTURECONVERTER_FORMATS_H */
