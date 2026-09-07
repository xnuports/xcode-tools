/*
 * formats.c -- the pixel formats a Khronos container can name.
 *
 * The ASTC entries run in the order Khronos allocated them, which is also
 * the order TextureConverter lists them in: the LDR block sizes from 4x4 to
 * 12x12, then the sRGB aliases, then the same sizes again for HDR.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "formats.h"

/* The base internal formats, spelled as OpenGL does. */
#define	GL_RED		0x1903
#define	GL_RGB		0x1907
#define	GL_RGBA		0x1908
#define	GL_RG		0x8227

struct entry {
	const char	*name;
	uint32_t	 gl;
	uint32_t	 vk;
	uint32_t	 base;			/* glBaseInternalFormat */
	int		 block_x, block_y;	/* 1x1 when uncompressed */
	uint32_t	 metal;			/* MTLPixelFormat, 0 if none */
};

static const struct entry table[] = {
	/* Uncompressed.  Only the float formats are ever written by the
	 * conversion path, but a container from elsewhere may name others. */
	{ "RGBA32",	0x8814, 109, GL_RGBA, 1, 1, 0 },	/* GL_RGBA32F, VK_..R32G32B32A32_SFLOAT */
	{ "RGB32",	0x8815, 106, GL_RGB,  1, 1, 0 },
	{ "RG32",	0x8230, 103, GL_RG,   1, 1, 0 },
	{ "R32",	0x822E, 100, GL_RED,  1, 1, 0 },
	{ "RGBA16",	0x881A, 97,  GL_RGBA, 1, 1, 0 },
	{ "RGB16",	0x881B, 90,  GL_RGB,  1, 1, 0 },
	{ "RG16",	0x822F, 83,  GL_RG,   1, 1, 0 },
	{ "R16",	0x822D, 76,  GL_RED,  1, 1, 0 },
	{ "RGBA8",	0x8058, 37,  GL_RGBA, 1, 1, 0 },
	{ "RGB8",	0x8051, 23,  GL_RGB,  1, 1, 0 },
	{ "RG8",	0x822B, 16,  GL_RG,   1, 1, 0 },
	{ "R8",		0x8229, 9,   GL_RED,  1, 1, 0 },
	{ "BGRA8",	0x93A1, 44,  GL_RGBA, 1, 1, 0 },

	/* ASTC, LDR. */
	{ "ASTC4x4",	0x93B0, 157, GL_RGBA, 4, 4, 204 },
	{ "ASTC5x4",	0x93B1, 159, GL_RGBA, 5, 4, 205 },
	{ "ASTC5x5",	0x93B2, 161, GL_RGBA, 5, 5, 206 },
	{ "ASTC6x5",	0x93B3, 163, GL_RGBA, 6, 5, 207 },
	{ "ASTC6x6",	0x93B4, 165, GL_RGBA, 6, 6, 208 },
	{ "ASTC8x5",	0x93B5, 167, GL_RGBA, 8, 5, 210 },
	{ "ASTC8x6",	0x93B6, 169, GL_RGBA, 8, 6, 211 },
	{ "ASTC8x8",	0x93B7, 171, GL_RGBA, 8, 8, 212 },
	{ "ASTC10x5",	0x93B8, 173, GL_RGBA, 10, 5, 213 },
	{ "ASTC10x6",	0x93B9, 175, GL_RGBA, 10, 6, 214 },
	{ "ASTC10x8",	0x93BA, 177, GL_RGBA, 10, 8, 215 },
	{ "ASTC10x10",	0x93BB, 179, GL_RGBA, 10, 10, 216 },
	{ "ASTC12x10",	0x93BC, 181, GL_RGBA, 12, 10, 217 },
	{ "ASTC12x12",	0x93BD, 183, GL_RGBA, 12, 12, 218 },

	/* BC.  BC4 carries one channel and BC5 two, and BC6 is colour with
	 * no alpha, so these are the three that are not GL_RGBA. */
	{ "BC1",	0x83F1, 133, GL_RGBA, 4, 4, 0 },
	{ "BC2",	0x83F2, 135, GL_RGBA, 4, 4, 0 },
	{ "BC3",	0x83F3, 137, GL_RGBA, 4, 4, 0 },
	{ "BC4",	0x8DBB, 139, GL_RED,  4, 4, 0 },
	{ "BC5",	0x8DBD, 141, GL_RG,   4, 4, 0 },
	{ "BC6U",	0x8E8F, 143, GL_RGB,  4, 4, 0 },
	{ "BC6S",	0x8E8E, 144, GL_RGB,  4, 4, 0 },
	{ "BC7",	0x8E8C, 145, GL_RGBA, 4, 4, 0 },

	/* ETC2 and EAC, the same way: R11 is one channel, RG11 two, and
	 * ETC2_RGB8 has no alpha. */
	{ "ETC2_RGB8",	0x9274, 147, GL_RGB,  4, 4, 0 },
	{ "ETC2_RGB8A1", 0x9276, 151, GL_RGBA, 4, 4, 0 },
	{ "EAC_RGBA8",	0x9278, 149, GL_RGBA, 4, 4, 0 },
	{ "EAC_R11",	0x9270, 153, GL_RED,  4, 4, 0 },
	{ "EAC_RG11",	0x9272, 155, GL_RG,   4, 4, 0 },

	{ NULL,		0, 0, 0, 0, 0, 0 }
};

const char *
format_name_for_gl(uint32_t gl)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (e->gl == gl)
			return (e->name);
	}
	return (NULL);
}

const char *
format_name_for_vk(uint32_t vk)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (e->vk == vk)
			return (e->name);
	}
	return (NULL);
}

_Bool
format_lookup(const char *name, uint32_t *gl, uint32_t *base, int *block_x,
    int *block_y, uint32_t *metal)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (strcmp(e->name, name) != 0)
			continue;
		*gl = e->gl;
		*base = e->base;
		*block_x = e->block_x;
		*block_y = e->block_y;
		*metal = e->metal;
		return (1);
	}
	return (0);
}

_Bool
format_is_float(const char *name)
{
	if (name == NULL)
		return (0);
	return (strcmp(name, "RGBA32") == 0 || strcmp(name, "RGB32") == 0 ||
	    strcmp(name, "RG32") == 0 || strcmp(name, "R32") == 0 ||
	    strcmp(name, "RGBA16") == 0 || strcmp(name, "RGB16") == 0 ||
	    strcmp(name, "RG16") == 0 || strcmp(name, "R16") == 0);
}
