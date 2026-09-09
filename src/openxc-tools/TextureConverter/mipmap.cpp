/*
 * mipmap.cpp -- the mip chain, built the way Apple's tool builds it.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nvimage/FloatImage.h"
#include "nvimage/Filter.h"

#include <cstdlib>
#include <cstring>

#include "mipmap.h"

float *
mip_downsample(const float *rgba, int w, int h, int d, enum mip_filter which,
    enum mip_wrap wrap, int *out_w, int *out_h, int *out_d)
{
	nv::FloatImage::WrapMode mode =
	    wrap == MIP_WRAP_CLAMP ? nv::FloatImage::WrapMode_Clamp :
	    wrap == MIP_WRAP_REPEAT ? nv::FloatImage::WrapMode_Repeat :
	    nv::FloatImage::WrapMode_Mirror;
	nv::FloatImage img;
	nv::FloatImage *half = NULL;
	float *out = NULL;
	int nw = 0, nh = 0, nd, x, y, z, c;

	if (w <= 1 && h <= 1)
		return (NULL);
	if (d < 1)
		d = 1;
	nd = d > 1 ? d / 2 : 1;

	/*
	 * A volume is halved a slice at a time and then the slices are
	 * paired off, which is not what a three dimensional kernel would
	 * do: Apple's level is the plain average of two slices that have
	 * each been through the two dimensional filter, and an odd slice
	 * at the end is dropped rather than carried.
	 */
	for (z = 0; z < (d > 1 ? nd * 2 : 1); z++) {
	const float *slice = rgba + (size_t)z * w * h * 4;

	img.allocate(4, (unsigned)w, (unsigned)h);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < 4; c++)
				img.pixel((unsigned)c, (unsigned)x,
				    (unsigned)y, 0) =
				    slice[((size_t)y * w + x) * 4 + c];
		}
	}

	/*
	 * --wrap_mode says what the filter reads past an edge, and Mirror is
	 * the default -- which is what the impulse response showed their
	 * filter doing.
	 */
	switch (which) {
	case MIP_FILTER_BOX:
		/*
		 * fastDownSample, not downSample(BoxFilter).  The two agree
		 * to within a unit in the last place and Apple's answer is
		 * this one: their Box chain matches it exactly, where the
		 * polyphase path is off by one ULP in a few dozen samples.
		 * It is the same average, summed in a different order.
		 *
		 */
		half = img.fastDownSample();
		break;
	case MIP_FILTER_TRIANGLE: {
		nv::TriangleFilter f;

		half = img.downSample(f, mode);
		break;
	}
	default: {
		nv::KaiserFilter f(3.0f);

		half = img.downSample(f, mode);
		break;
	}
	}
	if (half == NULL)
		return (NULL);

	nw = (int)half->width();
	nh = (int)half->height();
	if (out == NULL) {
		out = (float *)calloc((size_t)nw * nh * nd * 4,
		    sizeof(*out));
		if (out == NULL) {
			delete half;
			return (NULL);
		}
	}
	{
		float *dst = out + (size_t)(z / 2) * nw * nh * 4;

		for (y = 0; y < nh; y++) {
			for (x = 0; x < nw; x++) {
				for (c = 0; c < 4; c++) {
					float v = half->pixel((unsigned)c,
					    (unsigned)x, (unsigned)y, 0);
					float *o = &dst[((size_t)y * nw + x) *
					    4 + c];

					*o = (z & 1) ? (*o + v) * 0.5f : v;
				}
			}
		}
	}
	delete half;
	}
	*out_w = nw;
	*out_h = nh;
	if (out_d != NULL)
		*out_d = nd;
	return (out);
}

/*
 * --alpha_to_coverage, which is nvimage's, both halves of it.
 *
 * A mip chain loses coverage: filtering an alpha channel that a shader is
 * going to threshold makes the thresholded area shrink, and a leaf texture
 * thins out as it recedes.  The cure is to scale each level's alpha so that
 * the fraction of it above the reference matches the base's, and NVTT finds
 * that scale by bisecting over [0, 4] from a start of one, ten steps, and
 * keeping whichever step came closest rather than the last.
 *
 * The measure is not a count of texels above the reference.  It bilinearly
 * samples each 2x2 of neighbours sixteen times and counts the samples,
 * which is what a magnified texture is actually thresholded at, so a level
 * narrower than two texels has no measure at all and is left alone.
 *
 * The reference is --alpha_reference, 0.95 unless it is given.
 */
extern "C" float
mip_alpha_coverage(const float *rgba, int w, int h, float ref)
{
	nv::FloatImage img;
	int x, y, c;

	if (w < 2 || h < 2)
		return (-1.0f);
	img.allocate(4, (unsigned)w, (unsigned)h);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < 4; c++)
				img.pixel((unsigned)c, (unsigned)x,
				    (unsigned)y, 0) =
				    rgba[((size_t)y * w + x) * 4 + c];
		}
	}
	return (img.alphaTestCoverage(ref, 3));
}

extern "C" void
mip_scale_alpha_to_coverage(float *rgba, int w, int h, float desired,
    float ref)
{
	nv::FloatImage img;
	int x, y, c;

	if (w < 2 || h < 2 || desired < 0.0f)
		return;
	img.allocate(4, (unsigned)w, (unsigned)h);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < 4; c++)
				img.pixel((unsigned)c, (unsigned)x,
				    (unsigned)y, 0) =
				    rgba[((size_t)y * w + x) * 4 + c];
		}
	}
	img.scaleAlphaToCoverage(desired, ref, 3);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			rgba[((size_t)y * w + x) * 4 + 3] =
			    img.pixel(3, (unsigned)x, (unsigned)y, 0);
	}
}
