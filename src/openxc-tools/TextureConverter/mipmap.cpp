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
mip_downsample(const float *rgba, int w, int h, enum mip_filter which,
    int *out_w, int *out_h)
{
	nv::FloatImage img;
	nv::FloatImage *half = NULL;
	float *out;
	int nw, nh, x, y, c;

	if (w <= 1 && h <= 1)
		return (NULL);

	img.allocate(4, (unsigned)w, (unsigned)h);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < 4; c++)
				img.pixel((unsigned)c, (unsigned)x,
				    (unsigned)y, 0) =
				    rgba[((size_t)y * w + x) * 4 + c];
		}
	}

	/*
	 * Mirror at the edges, which is what Apple's default wrap mode is and
	 * what the impulse response showed their filter doing.
	 */
	switch (which) {
	case MIP_FILTER_BOX: {
		nv::BoxFilter f;

		half = img.downSample(f, nv::FloatImage::WrapMode_Mirror);
		break;
	}
	case MIP_FILTER_TRIANGLE: {
		nv::TriangleFilter f;

		half = img.downSample(f, nv::FloatImage::WrapMode_Mirror);
		break;
	}
	default: {
		nv::KaiserFilter f(3.0f);

		half = img.downSample(f, nv::FloatImage::WrapMode_Mirror);
		break;
	}
	}
	if (half == NULL)
		return (NULL);

	nw = (int)half->width();
	nh = (int)half->height();
	out = (float *)malloc((size_t)nw * nh * 4 * sizeof(*out));
	if (out == NULL) {
		delete half;
		return (NULL);
	}
	for (y = 0; y < nh; y++) {
		for (x = 0; x < nw; x++) {
			for (c = 0; c < 4; c++)
				out[((size_t)y * nw + x) * 4 + c] =
				    half->pixel((unsigned)c, (unsigned)x,
				    (unsigned)y, 0);
		}
	}
	delete half;
	*out_w = nw;
	*out_h = nh;
	return (out);
}
