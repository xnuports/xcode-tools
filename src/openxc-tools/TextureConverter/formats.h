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

#endif /* TEXTURECONVERTER_FORMATS_H */
