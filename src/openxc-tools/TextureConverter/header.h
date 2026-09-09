/*
 * header.h -- writing the levels as a C header.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_HEADER_H
#define TEXTURECONVERTER_HEADER_H

#include <stddef.h>

/*
 * The levels as C, named after the output file's stem.  Returns a malloc'd
 * NUL-terminated string, or NULL if it could not be built.
 */
char	*header_write(void **levels, const size_t *sizes, const int *widths,
	    const int *heights, int nlevels, const char *name,
	    const char *atc_format, const char *gamut, const char *ident,
	    _Bool srgb, _Bool normal, int faces, _Bool array,
	    const int *depths,
	    size_t *out_len);

#endif /* TEXTURECONVERTER_HEADER_H */
