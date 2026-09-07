/*
 * compress.h -- block compression, through the encoders Apple's tool names.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_COMPRESS_H
#define TEXTURECONVERTER_COMPRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * What Apple's four quality names ask astcenc for.  Measured by compressing
 * the same image both ways and comparing blocks: the names are astcenc's
 * own presets, three of them the obvious ones and Highest the exhaustive
 * search rather than the very-thorough one below it.
 */
enum tc_quality {
	TC_QUALITY_FASTEST,
	TC_QUALITY_NORMAL,
	TC_QUALITY_PRODUCTION,
	TC_QUALITY_HIGHEST
};

struct tc_astc_options {
	int		block_x, block_y;
	enum tc_quality	quality;
	bool		perceptual;	/* --channel_weighting=Perceptual */
	bool		alpha_weight;	/* --alpha_weight */
};

/*
 * Compress one tightly packed RGBA float image.  The caller owns the result
 * and frees it with free(); *out_len is the byte count.  Returns NULL when
 * the encoder refuses the settings.
 */
uint8_t	*compress_astc(const float *rgba, int w, int h,
	    const struct tc_astc_options *, size_t *out_len);

#endif /* TEXTURECONVERTER_COMPRESS_H */
