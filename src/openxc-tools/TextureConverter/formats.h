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

#include <stddef.h>
#include <stdint.h>

/* NULL when the enumerant is one this tool has no name for. */
const char *format_name_for_gl(uint32_t gl_internal_format);
const char *format_name_for_vk(uint32_t vk_format);

/* True for a format whose samples are floating point, which is what makes
 * the colour space extended-range linear rather than plain sRGB.  The
 * sixteen bit formats are half floats rather than unorms, so they count. */
_Bool	format_is_float(const char *name);

/* 8, 16 or 32 for an uncompressed format; 0 for anything else. */
int	format_channel_bits(const char *name);

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

/*
 * The OpenGL and Vulkan enumerants --srgb_format asks for.  False for a
 * format with no sRGB spelling, which is every one that carries no colour:
 * BC4, BC5, BC6, EAC_R11, EAC_RG11 and the float formats.
 */
_Bool	format_srgb_for(const char *name, uint32_t *gl, uint32_t *vk);

/* False only for a name this tool does not know at all. */
_Bool	format_dfd_for(const char *name, _Bool srgb, struct format_dfd *);

/*
 * The name AppleTextureConverter.h gives a format, which the .h output
 * writes.  The sRGB spelling is built in buf, so pass one big enough for
 * the longest name; the linear spellings are returned as they stand.
 */
const char *format_atc_for(const char *name, _Bool srgb, char *buf,
	    size_t buflen);

/* The channel count that output records, which is BC6's only oddity. */
int	format_atc_channels(const char *name, _Bool srgb);

/*
 * The DXGI enumerant a DDS file names the format with; zero for a format
 * Direct3D has no name for, which is what makes the tool refuse to write a
 * DDS for it.
 */
uint32_t format_dxgi_for(const char *name, _Bool srgb);

/* NULL for an enumerant this tool has no name for. */
const char *format_name_for_dxgi(uint32_t dxgi);

#endif /* TEXTURECONVERTER_FORMATS_H */
