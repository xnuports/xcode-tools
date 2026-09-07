/*
 * etc2.cpp -- the ETC2 and EAC block formats, through Google's etc2comp.
 *
 * Not a choice either: with --compressor=Auto, Apple's tool answers
 * "Using Compressor: ETC2COMP" for all five of these formats at every
 * quality, so this is the back end that has to be matched.
 *
 * etc2comp takes a tightly packed RGBA float image, which is what the mip
 * chain here already holds, so unlike NVTT there is nothing to transpose.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Etc.h>
#include <EtcImage.h>
#include <EtcErrorMetric.h>

#include <stdlib.h>
#include <string.h>

#include "compress.h"

namespace {

Etc::Image::Format
etc_format(enum tc_etc f)
{
	switch (f) {
	case TC_ETC2_RGB8:	return (Etc::Image::Format::RGB8);
	case TC_ETC2_RGB8A1:	return (Etc::Image::Format::RGB8A1);
	case TC_EAC_RGBA8:	return (Etc::Image::Format::RGBA8);
	case TC_EAC_R11:	return (Etc::Image::Format::R11);
	default:		return (Etc::Image::Format::RG11);
	}
}

/*
 * etc2comp's effort is a number from 0 to 100 rather than a set of named
 * presets, and Apple's four names land on four evenly spaced values.
 *
 * Measured by encoding the same image at every effort from 0 to 100 in
 * steps of five and asking which reproduce their blocks.  Production lands
 * on 80 alone and Highest on 95 or 100; Fastest and Normal both match
 * anything from 0 to 45, because below some threshold the encoder does the
 * same work either way.  0, 40, 80, 100 is the reading that fits all four
 * and is the spacing the numbers suggest.
 */
float
etc_effort(enum tc_quality q)
{
	switch (q) {
	case TC_QUALITY_FASTEST:	return (0.0f);
	case TC_QUALITY_NORMAL:		return (40.0f);
	case TC_QUALITY_HIGHEST:	return (100.0f);
	default:			return (80.0f);
	}
}

} /* namespace */

extern "C" uint8_t *
compress_etc(const float *rgba, int w, int h, enum tc_etc fmt,
    enum tc_quality quality, bool perceptual, size_t *out_len)
{
	unsigned char *bits = NULL;
	unsigned int nbytes = 0, ew = 0, eh = 0;
	int ms = 0;
	uint8_t *out;

	/*
	 * REC709 for everything.  That is not the same distinction
	 * --channel_weighting draws elsewhere, and it is what Apple do: all
	 * five formats match their blocks under REC709 and none under RGBA.
	 * R11 and RG11 carry no colour and come out the same whatever is
	 * passed, so they say nothing either way.
	 */
	Etc::ErrorMetric metric = Etc::REC709;

	(void)perceptual;

	Etc::Encode((float *)(uintptr_t)rgba, (unsigned)w, (unsigned)h,
	    etc_format(fmt), metric, etc_effort(quality), 1, 1,
	    &bits, &nbytes, &ew, &eh, &ms, false);
	if (bits == NULL)
		return (NULL);

	/* The caller frees with free(), so the buffer is handed over as one. */
	if ((out = (uint8_t *)malloc(nbytes)) != NULL)
		memcpy(out, bits, nbytes);
	delete[] bits;
	if (out == NULL)
		return (NULL);
	*out_len = nbytes;
	return (out);
}
