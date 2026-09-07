/*
 * eac.c -- decoding the EAC single- and dual-channel formats.
 *
 * NVTT's decoder has the call sites for these commented out and marked
 * "@@ Not implemented", and etc2comp has no decoder at all, so neither
 * library will read back what they wrote.  EAC is small and completely
 * specified, so it is written here rather than left as a hole.
 *
 * A block is sixty-four bits, most significant first: an eight bit base
 * codeword, a four bit multiplier, a four bit table index, and then sixteen
 * three bit selectors.  The selectors run down each column before moving
 * right, which is the one thing about the format that surprises people.
 *
 * OpenGL ES 3.0, 3.8.15.  EAC_RG11 is two of these blocks side by side, red
 * then green.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "decode.h"

/* The sixteen modifier sets, from the table in the specification. */
static const int modifiers[16][8] = {
	{ -3, -6,  -9, -15, 2, 5, 8, 14 },
	{ -3, -7, -10, -13, 2, 6, 9, 12 },
	{ -2, -5,  -8, -13, 1, 4, 7, 12 },
	{ -2, -4,  -6, -13, 1, 3, 5, 12 },
	{ -3, -6,  -8, -12, 2, 5, 7, 11 },
	{ -3, -7,  -9, -11, 2, 6, 8, 10 },
	{ -4, -7,  -8, -11, 3, 6, 7, 10 },
	{ -3, -5,  -8, -11, 2, 4, 7, 10 },
	{ -2, -6,  -8, -10, 1, 5, 7,  9 },
	{ -2, -5,  -8, -10, 1, 4, 7,  9 },
	{ -2, -4,  -8, -10, 1, 3, 7,  9 },
	{ -2, -5,  -7, -10, 1, 4, 6,  9 },
	{ -3, -4,  -7, -10, 2, 3, 6,  9 },
	{ -1, -2,  -3, -10, 0, 1, 2,  9 },
	{ -4, -6,  -8,  -9, 3, 5, 7,  8 },
	{ -3, -5,  -7,  -9, 2, 4, 6,  8 }
};

/*
 * Eleven bits down to eight, and back out as the float that names that byte
 * exactly.
 *
 * It goes through sixteen: the eleven bits are replicated up to a sixteen
 * bit channel, which is what maps 2047 onto 65535 rather than onto 65504,
 * and only then narrowed to eight -- truncating, since sixteen bit unorm
 * divided by 257 is the byte.  Going straight from eleven to eight is one
 * too low for a sample here and there, 1172 being the first.
 *
 * The narrowing belongs here rather than in the caller, which rounds: a
 * sample is already a multiple of 1/255 by the time it leaves.
 */
static float
to_unorm8(int v)
{
	int v16 = (v << 5) | (v >> 6);

	return ((float)(v16 / 257) * (1.0f / 255.0f));
}

/*
 * One block into sixteen values in [0, 2047], in raster order.
 */
static void
eac_block(const uint8_t *b, int out[16])
{
	uint64_t bits = 0;
	int base, mult, table;
	int x, y;
	size_t i;

	for (i = 0; i < 8; i++)
		bits = (bits << 8) | b[i];

	base = (int)((bits >> 56) & 0xff);
	mult = (int)((bits >> 52) & 0xf);
	table = (int)((bits >> 48) & 0xf);

	/*
	 * A multiplier of zero means one rather than zero -- otherwise every
	 * selector would decode to the same value and the block could hold
	 * only a single level.
	 */
	mult = mult == 0 ? 1 : mult * 8;

	for (x = 0; x < 4; x++) {
		for (y = 0; y < 4; y++) {
			int shift = 45 - (x * 4 + y) * 3;
			int sel = (int)((bits >> shift) & 0x7);
			int v = base * 8 + 4 + modifiers[table][sel] * mult;

			if (v < 0)
				v = 0;
			if (v > 2047)
				v = 2047;
			out[y * 4 + x] = v;
		}
	}
}

float *
decode_eac(const uint8_t *blocks, size_t len, int w, int h, bool two)
{
	int bw = (w + 3) / 4, bh = (h + 3) / 4;
	size_t bs = two ? 16 : 8;
	float *out;
	int bx, by;

	if (len < (size_t)bw * (size_t)bh * bs)
		return (NULL);
	if ((out = calloc((size_t)w * h * 4, sizeof(*out))) == NULL)
		return (NULL);

	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			const uint8_t *p = blocks +
			    ((size_t)by * bw + bx) * bs;
			int r[16], g[16];
			int x, y;

			eac_block(p, r);
			if (two)
				eac_block(p + 8, g);

			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int px = bx * 4 + x, py = by * 4 + y;
					float *o;

					if (px >= w || py >= h)
						continue;
					o = out + ((size_t)py * w + px) * 4;
					o[0] = to_unorm8(r[y * 4 + x]);
					o[1] = two ?
					    to_unorm8(g[y * 4 + x]) : 0.0f;
					o[2] = 0.0f;
					o[3] = 1.0f;
				}
			}
		}
	}
	return (out);
}
