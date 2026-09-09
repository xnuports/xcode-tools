/*
 * decode.h -- turning compressed blocks back into pixels.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEXTURECONVERTER_DECODE_H
#define TEXTURECONVERTER_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The formats NVTT's block decoder understands, by its names for them. */
enum tc_decode {
	TC_DEC_BC1, TC_DEC_BC2, TC_DEC_BC3, TC_DEC_BC4, TC_DEC_BC5,
	TC_DEC_BC6, TC_DEC_BC6S, TC_DEC_BC7,
	TC_DEC_ETC2_R, TC_DEC_ETC2_RG, TC_DEC_ETC2_RGB, TC_DEC_ETC2_RGBA
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Both return one tightly packed RGBA float image, w*h*4 floats, which the
 * caller frees with free(); NULL when the blocks cannot be read.
 */
float	*decode_astc(const uint8_t *blocks, size_t len, int w, int h,
	    int block_x, int block_y);

/*
 * The same, decoded straight to eight bits rather than to float and rounded
 * after.  astcenc's own quantisation is not the same as rounding its float
 * output, and Apple's answer is astcenc's: w*h*4 bytes, freed with free().
 */
uint8_t	*decode_astc_u8(const uint8_t *blocks, size_t len, int w, int h,
	    int block_x, int block_y);
float	*decode_blocks(const uint8_t *blocks, size_t len, int w, int h,
	    enum tc_decode);

/*
 * EAC_R11 and EAC_RG11, which neither library here will read back.  "two"
 * selects the dual-channel form.  See eac.c.
 */
float	*decode_eac(const uint8_t *blocks, size_t len, int w, int h,
	    bool two);

/* Whether a BC7 level holds a mode 0 block, which NVTT cannot read. */
bool	 bc7_has_mode0(const uint8_t *blocks, size_t len);

/* An image file through NVTT's reader.  See decode.cpp. */
uint8_t	*image_load_rgba8(const char *path, int *wp, int *hp);

#ifdef __cplusplus
}
#endif

#endif /* TEXTURECONVERTER_DECODE_H */
