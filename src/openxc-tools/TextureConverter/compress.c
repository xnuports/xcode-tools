/*
 * compress.c -- block compression, through the encoders Apple's tool names.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <astcenc.h>

#include <stdlib.h>
#include <string.h>

#include "compress.h"

/*
 * astcenc takes quality as a number, with named presets at particular
 * values.  Apple's four names land on four of them; Highest is the
 * exhaustive search at 100 rather than the very-thorough one at 99, which
 * only shows up on an image hard enough to separate them.
 */
static float
astc_quality(enum tc_quality q)
{
	switch (q) {
	case TC_QUALITY_FASTEST:	return (ASTCENC_PRE_FASTEST);
	case TC_QUALITY_NORMAL:		return (ASTCENC_PRE_FAST);
	case TC_QUALITY_HIGHEST:	return (ASTCENC_PRE_EXHAUSTIVE);
	default:			return (ASTCENC_PRE_MEDIUM);
	}
}

uint8_t *
compress_astc(const float *rgba, int w, int h,
    const struct tc_astc_options *opt, size_t *out_len)
{
	/*
	 * A normal map is not stored as a colour.  astcenc's normal mode
	 * keeps two channels and reconstructs the third, and it wants them
	 * where an ASTC block holds two channels best: x replicated across
	 * the colour and y in alpha, which is the "rrrg" swizzle its own
	 * tool uses for -normal.  Anything else -- including the identity
	 * swizzle with the same flag -- gives different blocks.
	 */
	static const struct astcenc_swizzle sw_rgba = {
		ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A
	};
	static const struct astcenc_swizzle sw_rrrg = {
		ASTCENC_SWZ_R, ASTCENC_SWZ_R, ASTCENC_SWZ_R, ASTCENC_SWZ_G
	};
	const struct astcenc_swizzle *swizzle = opt->normal ? &sw_rrrg :
	    &sw_rgba;
	struct astcenc_config config;
	struct astcenc_context *ctx = NULL;
	struct astcenc_image image;
	void *slice = (void *)(uintptr_t)rgba;
	unsigned flags = 0;
	uint8_t *out;
	size_t blocks, len;

	/*
	 * Perceptual weighting is on unless asked otherwise, because
	 * --channel_weighting defaults to Perceptual.
	 */
	if (opt->perceptual)
		flags |= ASTCENC_FLG_USE_PERCEPTUAL;
	/*
	 * Normal mode and alpha weighting do not go together: alpha is
	 * carrying a coordinate there, not coverage, and asking astcenc to
	 * weigh by it moves sixteen bytes of every block.  Apple do not.
	 */
	if (opt->normal)
		flags |= ASTCENC_FLG_MAP_NORMAL;
	else if (opt->alpha_weight)
		flags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;
	/*
	 * RGBM keeps the multiplier in alpha, and astcenc has to be told:
	 * its heuristics and its error metric both change, and it wants the
	 * reconstruction scale as well.  Six is the scale the encoding step
	 * uses, and astcenc's own -rgbm weighs alpha at twice it.
	 *
	 * The flag and a normal map do not go together -- astcenc refuses
	 * the pair -- but the scale and the weight still take effect, and
	 * Apple set them there too.
	 */
	if (opt->rgbm && !opt->normal)
		flags |= ASTCENC_FLG_MAP_RGBM;

	if (astcenc_config_init(ASTCENC_PRF_LDR, (unsigned)opt->block_x,
	    (unsigned)opt->block_y, 1, astc_quality(opt->quality), flags,
	    &config) != ASTCENC_SUCCESS)
		return (NULL);
	if (opt->rgbm) {
		config.rgbm_m_scale = opt->rgbm_range;
		config.cw_a_weight = 2.0f * opt->rgbm_range;
	}
	if (astcenc_context_alloc(&config, 1, &ctx, NULL) != ASTCENC_SUCCESS)
		return (NULL);

	memset(&image, 0, sizeof(image));
	image.dim_x = (unsigned)w;
	image.dim_y = (unsigned)h;
	image.dim_z = 1;
	image.data_type = ASTCENC_TYPE_F32;
	image.data = &slice;

	blocks = (size_t)((w + opt->block_x - 1) / opt->block_x) *
	    (size_t)((h + opt->block_y - 1) / opt->block_y);
	len = blocks * 16;
	if ((out = malloc(len)) == NULL) {
		astcenc_context_free(ctx);
		return (NULL);
	}
	if (astcenc_compress_image(ctx, &image, swizzle, out, len, 0) !=
	    ASTCENC_SUCCESS) {
		free(out);
		astcenc_context_free(ctx);
		return (NULL);
	}
	astcenc_context_free(ctx);
	*out_len = len;
	return (out);
}
