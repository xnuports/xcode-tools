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

#endif /* TEXTURECONVERTER_DDS_H */
