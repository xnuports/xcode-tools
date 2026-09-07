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
 * The OpenGL internal format a name compresses to, the block it packs
 * pixels into, and the MTLPixelFormat a Khronos container records beside
 * it (zero for a format Apple record none for).  Returns false for a name
 * this tool does not write.
 */
_Bool	format_lookup(const char *name, uint32_t *gl, int *block_x,
	    int *block_y, uint32_t *metal);

#endif /* TEXTURECONVERTER_FORMATS_H */
