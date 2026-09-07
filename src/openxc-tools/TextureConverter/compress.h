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
	bool		normal;		/* --normal_map */
};

/*
 * The BC formats, which go through NVTT.  BC6 is one format there under two
 * names; the signed and unsigned variants are separate here because that is
 * how Apple's tool names them.
 */
enum tc_bc {
	TC_BC1, TC_BC1A, TC_BC2, TC_BC3, TC_BC3N, TC_BC4, TC_BC5, TC_BC6U,
	TC_BC6S, TC_BC7
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compress one tightly packed RGBA float image.  The caller owns the result
 * and frees it with free(); *out_len is the byte count.  Returns NULL when
 * the encoder refuses the settings.
 */
uint8_t	*compress_astc(const float *rgba, int w, int h,
	    const struct tc_astc_options *, size_t *out_len);

/* The same contract, for the BC family.  See nvtt.cpp. */
uint8_t	*compress_bc(const float *rgba, int w, int h, enum tc_bc,
	    enum tc_quality, size_t *out_len);

/* The ETC2 and EAC formats, which all go through etc2comp. */
enum tc_etc {
	TC_ETC2_RGB8, TC_ETC2_RGB8A1, TC_EAC_RGBA8, TC_EAC_R11, TC_EAC_RG11
};

/* The same contract again, for those five.  See etc2.cpp. */
uint8_t	*compress_etc(const float *rgba, int w, int h, enum tc_etc,
	    enum tc_quality, bool perceptual, size_t *out_len);

/* And through stb_dxt, which Apple reach for BC1 at Highest.  See stb.c. */
uint8_t	*compress_bc_stb(const float *rgba, int w, int h, enum tc_bc,
	    enum tc_quality, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURECONVERTER_COMPRESS_H */
