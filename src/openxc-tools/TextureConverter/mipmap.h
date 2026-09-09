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

/* --wrap_mode, which says what the filter reads past an edge. */
enum mip_wrap {
	MIP_WRAP_MIRROR,
	MIP_WRAP_CLAMP,
	MIP_WRAP_REPEAT
};

/*
 * Halve a tightly packed RGBA float image, slice by slice in z as well:
 * --build_volume hands this a depth and the chain halves that too, so a
 * 8x8x4 volume gives 4x4x2 and then 2x2x1.  The caller owns the result and
 * frees it with free().  Returns NULL when there is nothing left to halve.
 */
float	*mip_downsample(const float *rgba, int w, int h, int d,
	    enum mip_filter, enum mip_wrap, int *out_w, int *out_h,
	    int *out_d);

/* --alpha_to_coverage, through nvimage.  See mipmap.cpp. */
float	 mip_alpha_coverage(const float *rgba, int w, int h, float ref);
void	 mip_scale_alpha_to_coverage(float *rgba, int w, int h, float desired,
	    float ref);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURECONVERTER_MIPMAP_H */
