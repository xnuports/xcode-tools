/*
 * stb.c -- BC1, BC3, BC4 and BC5 through stb_dxt.
 *
 * Not the usual back end: with --compressor=Auto, which is the default,
 * Apple's tool sends every BC format to NVTT except one -- BC1 at
 * --compression_quality=Highest, which goes to STB.  So this exists to be
 * that one case, and to be there when --compressor=STB is asked for.
 *
 * stb_dxt works a block at a time on 4x4 RGBA bytes, so the float image is
 * quantised and gathered into blocks here.  Blocks at the right and bottom
 * edges of an image that is not a multiple of four repeat the last row and
 * column, which is what every encoder in this tool does with a partial
 * block.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * string.h first: stb_dxt.h's implementation calls memcpy and declares
 * nothing itself.
 */
#define STB_DXT_IMPLEMENTATION
#define STB_DXT_STATIC
#include <stb_dxt.h>

#include "compress.h"

/*
 * One 4x4 block of RGBA bytes, gathered from the float image.
 *
 * Both details here are NVTT's, from ColorBlock::init, because that is what
 * Apple's tool quantises with before it hands anything to an encoder:
 *
 *   - a block that runs off the edge repeats the pixels it does have, and
 *     repeats them cyclically rather than clamping to the last one, so a
 *     2x2 image fills its block as 0 1 0 1 rather than 0 1 1 1;
 *   - the float is quantised by truncation, not rounding.  It makes no
 *     difference at level 0, where every sample came from a byte and lands
 *     exactly back on it, and it decides several blocks in every mip level
 *     below that.
 */
static void
gather(const float *rgba, int w, int h, int bx, int by, unsigned char *out)
{
	int bw = w - bx < 4 ? w - bx : 4;
	int bh = h - by < 4 ? h - by : 4;
	int x, y, c;

	for (y = 0; y < 4; y++) {
		int sy = by + y % bh;

		for (x = 0; x < 4; x++) {
			const float *p = rgba +
			    ((size_t)sy * w + bx + x % bw) * 4;

			for (c = 0; c < 4; c++) {
				float v = p[c];

				if (v < 0.0f)
					v = 0.0f;
				if (v > 1.0f)
					v = 1.0f;
				out[(y * 4 + x) * 4 + c] =
				    (unsigned char)(255.0f * v);
			}
		}
	}
}

uint8_t *
compress_bc_stb(const float *rgba, int w, int h, enum tc_bc fmt,
    enum tc_quality quality, size_t *out_len)
{
	int bw = (w + 3) / 4, bh = (h + 3) / 4;
	int bs, bx, by, mode;
	uint8_t *out, *p;

	switch (fmt) {
	case TC_BC1:
	case TC_BC1A:
	case TC_BC4:	bs = 8; break;
	case TC_BC3:
	case TC_BC3N:
	case TC_BC5:	bs = 16; break;
	default:	return (NULL);	/* stb writes no other format */
	}

	/*
	 * stb's one quality knob is whether to run a second refinement pass,
	 * and Apple spend it only at Highest: Fastest, Normal and Production
	 * all come out of their tool matching the single-pass mode.  That is
	 * also the only quality Auto sends BC1 here at.
	 */
	mode = quality == TC_QUALITY_HIGHEST ? STB_DXT_HIGHQUAL :
	    STB_DXT_NORMAL;

	if ((out = malloc((size_t)bw * bh * bs)) == NULL)
		return (NULL);
	p = out;
	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			unsigned char block[64], one[16], two[32];
			int i;

			gather(rgba, w, h, bx * 4, by * 4, block);
			switch (fmt) {
			case TC_BC4:
				for (i = 0; i < 16; i++)
					one[i] = block[i * 4];
				stb_compress_bc4_block(p, one);
				break;
			case TC_BC5:
				for (i = 0; i < 16; i++) {
					two[i * 2] = block[i * 4];
					two[i * 2 + 1] = block[i * 4 + 1];
				}
				stb_compress_bc5_block(p, two);
				break;
			case TC_BC3:
			case TC_BC3N:
				stb_compress_dxt_block(p, block, 1, mode);
				break;
			default:
				stb_compress_dxt_block(p, block, 0, mode);
				break;
			}
			p += bs;
		}
	}
	*out_len = (size_t)bw * bh * bs;
	return (out);
}
