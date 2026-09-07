/*
 * ktx2.h -- writing Khronos texture container version 2.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_KTX2_H
#define TEXTURECONVERTER_KTX2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "formats.h"

/*
 * Levels largest first, as everything else in this tool passes them; the
 * writer reverses them.  "options" and "version" may be NULL, which is what
 * --disable_annotation asks for.  The caller frees the result.
 */
uint8_t	*ktx2_write(void **levels, const size_t *sizes, const int *widths,
	    const int *heights, int nlevels, int faces, uint32_t vk_format,
	    int block_bytes, int block_x, int block_y, int type_size,
	    const struct format_dfd *, bool premultiplied, bool srgb,
	    const char *writer, const char *options, const char *version,
	    size_t *out_len);

#endif /* TEXTURECONVERTER_KTX2_H */
