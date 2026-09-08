/*
 * dds.h -- writing a DirectDraw surface.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_DDS_H
#define TEXTURECONVERTER_DDS_H

#include <stddef.h>
#include <stdint.h>

uint8_t	*dds_write(void **levels, const size_t *sizes, int width, int height,
	    int nlevels, uint32_t dxgi_format, size_t *out_len);

/*
 * The other direction, for a DDS handed in as an input.  Fills in the
 * shape and the DXGI enumerant and points at the first level, which is all
 * the reading path wants; false for anything that is not a DX10 DDS.
 */
_Bool	 dds_parse(const void *bytes, size_t len, uint32_t *dxgi,
	    int *width, int *height, int *nlevels, const uint8_t **data,
	    size_t *data_len);

#endif /* TEXTURECONVERTER_DDS_H */
