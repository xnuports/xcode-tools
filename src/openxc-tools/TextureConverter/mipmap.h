/*
 * mipmap.h -- the mip chain, built the way Apple's tool builds it.
 *
 * Their filters are NVTT's: an impulse through their Kaiser gives exactly
 * the weights NVTT's polyphase kernel produces, and running NVTT's
 * downSample against their output reproduces every level bit for bit.  So
 * this is a thin C face on nvimage rather than a second implementation.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_MIPMAP_H
#define TEXTURECONVERTER_MIPMAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum mip_filter {
	MIP_FILTER_BOX,
	MIP_FILTER_TRIANGLE,
	MIP_FILTER_KAISER
};

/*
 * Halve a tightly packed RGBA float image.  The caller owns the result and
 * frees it with free().  Returns NULL when the image is already 1x1.
 */
float	*mip_downsample(const float *rgba, int w, int h, enum mip_filter,
	    int *out_w, int *out_h);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURECONVERTER_MIPMAP_H */
