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

struct entry {
	const char	*name;
	uint32_t	 gl;
	uint32_t	 vk;
};

static const struct entry table[] = {
	/* Uncompressed.  Only the float formats are ever written by the
	 * conversion path, but a container from elsewhere may name others. */
	{ "RGBA32",	0x8814, 109 },	/* GL_RGBA32F, VK_..R32G32B32A32_SFLOAT */
	{ "RGB32",	0x8815, 106 },
	{ "RG32",	0x8230, 103 },
	{ "R32",	0x822E, 100 },
	{ "RGBA16",	0x881A, 97 },
	{ "RGB16",	0x881B, 90 },
	{ "RG16",	0x822F, 83 },
	{ "R16",	0x822D, 76 },
	{ "RGBA8",	0x8058, 37 },
	{ "RGB8",	0x8051, 23 },
	{ "RG8",	0x822B, 16 },
	{ "R8",		0x8229, 9 },
	{ "BGRA8",	0x93A1, 44 },

	/* ASTC, LDR. */
	{ "ASTC4x4",	0x93B0, 157 },
	{ "ASTC5x4",	0x93B1, 159 },
	{ "ASTC5x5",	0x93B2, 161 },
	{ "ASTC6x5",	0x93B3, 163 },
	{ "ASTC6x6",	0x93B4, 165 },
	{ "ASTC8x5",	0x93B5, 167 },
	{ "ASTC8x6",	0x93B6, 169 },
	{ "ASTC8x8",	0x93B7, 171 },
	{ "ASTC10x5",	0x93B8, 173 },
	{ "ASTC10x6",	0x93B9, 175 },
	{ "ASTC10x8",	0x93BA, 177 },
	{ "ASTC10x10",	0x93BB, 179 },
	{ "ASTC12x10",	0x93BC, 181 },
	{ "ASTC12x12",	0x93BD, 183 },

	/* BC. */
	{ "BC1",	0x83F1, 133 },
	{ "BC2",	0x83F2, 135 },
	{ "BC3",	0x83F3, 137 },
	{ "BC4",	0x8DBB, 139 },
	{ "BC5",	0x8DBD, 141 },
	{ "BC6U",	0x8E8F, 143 },
	{ "BC6S",	0x8E8E, 144 },
	{ "BC7",	0x8E8C, 145 },

	/* ETC2 and EAC. */
	{ "ETC2_RGB8",	0x9274, 147 },
	{ "ETC2_RGB8A1", 0x9276, 151 },
	{ "EAC_RGBA8",	0x9278, 149 },
	{ "EAC_R11",	0x9270, 153 },
	{ "EAC_RG11",	0x9272, 155 },

	{ NULL,		0, 0 }
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
format_is_float(const char *name)
{
	if (name == NULL)
		return (0);
	return (strcmp(name, "RGBA32") == 0 || strcmp(name, "RGB32") == 0 ||
	    strcmp(name, "RG32") == 0 || strcmp(name, "R32") == 0 ||
	    strcmp(name, "RGBA16") == 0 || strcmp(name, "RGB16") == 0 ||
	    strcmp(name, "RG16") == 0 || strcmp(name, "R16") == 0);
}
