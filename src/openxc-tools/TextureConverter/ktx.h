/*
 * ktx.h -- reading Khronos texture containers.
 *
 * KTX version 1 is a fixed header of thirteen little-endian words followed
 * by a key/value block and then the mip levels, each preceded by its length.
 * Version 2 replaces that with an index of offsets and describes its format
 * with a Vulkan enumerant rather than an OpenGL one.  Both carry the same
 * thing in the end: a size, a pixel format and some number of mip levels.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_KTX_H
#define TEXTURECONVERTER_KTX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ktx_kv {
	char	*key;
	char	*value;		/* NUL terminated when it is text */
	size_t	 value_len;
};

/*
 * One mip level's payload, borrowed from the bytes handed to ktx_parse.
 * Version 1 stores the levels largest first, each behind its own length;
 * version 2 stores them smallest first behind an index, and they are
 * reordered here so that level 0 is the largest either way.
 */
struct ktx_level {
	const uint8_t	*data;
	size_t		 len;
	uint32_t	 width, height;
};

struct ktx {
	int		 version;	/* 1 or 2 */
	uint32_t	 width, height, depth;
	uint32_t	 layers, faces, levels;
	uint32_t	 gl_internal_format;	/* version 1 */
	uint32_t	 vk_format;		/* version 2 */
	struct ktx_kv	*kv;
	size_t		 nkv;
	struct ktx_level *level;
	size_t		 nlevel;
};

/* Returns false when the bytes are not a Khronos container at all. */
bool	ktx_parse(const void *bytes, size_t len, struct ktx *);
void	ktx_free(struct ktx *);

/* The value of a key, or NULL.  Borrowed from the parsed container. */
const char *ktx_value(const struct ktx *, const char *key);

#endif /* TEXTURECONVERTER_KTX_H */
