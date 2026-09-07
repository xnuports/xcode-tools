/*
 * nvtt.cpp -- the BC block formats, through NVIDIA Texture Tools.
 *
 * NVTT is not a choice: asked for any of BC1 through BC7 with the default
 * --compressor=Auto, Apple's tool prints "Using Compressor: NVTT".  stb and
 * ISPC are reachable only by naming them, so this is the back end that has
 * to be matched, and going through NVTT's own compressor is the only way to
 * be sure of the blocks rather than argue about them.
 *
 * NVTT's quality names are Apple's four, in the same order, so
 * --compression_quality passes straight through -- unlike ASTC, where they
 * had to be measured against astcenc's presets.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <nvtt/nvtt.h>

#include <stdlib.h>
#include <string.h>

#include "compress.h"

namespace {

/*
 * NVTT hands the blocks over as they are produced.  They are collected
 * rather than written, because the container this ends up in is ours.
 */
struct collector : public nvtt::OutputHandler {
	uint8_t	*buf;
	size_t	 len, cap;
	bool	 failed;

	collector() : buf(NULL), len(0), cap(0), failed(false) {}
	~collector() { free(buf); }

	void beginImage(int size, int, int, int, int, int)
	{
		if (size <= 0)
			return;
		free(buf);
		if ((buf = (uint8_t *)malloc((size_t)size)) == NULL)
			failed = true;
		cap = failed ? 0 : (size_t)size;
		len = 0;
	}

	bool writeData(const void *data, int size)
	{
		if (failed || size < 0 || len + (size_t)size > cap) {
			failed = true;
			return (false);
		}
		memcpy(buf + len, data, (size_t)size);
		len += (size_t)size;
		return (true);
	}

	void endImage() {}
};

nvtt::Format
nvtt_format(enum tc_bc f)
{
	switch (f) {
	case TC_BC1:	return (nvtt::Format_BC1);
	case TC_BC1A:	return (nvtt::Format_BC1a);
	case TC_BC2:	return (nvtt::Format_BC2);
	case TC_BC3:	return (nvtt::Format_BC3);
	case TC_BC3N:	return (nvtt::Format_BC3n);
	case TC_BC4:	return (nvtt::Format_BC4);
	case TC_BC5:	return (nvtt::Format_BC5);
	case TC_BC6U:
	case TC_BC6S:	return (nvtt::Format_BC6);
	default:	return (nvtt::Format_BC7);
	}
}

nvtt::Quality
nvtt_quality(enum tc_quality q)
{
	switch (q) {
	case TC_QUALITY_FASTEST:	return (nvtt::Quality_Fastest);
	case TC_QUALITY_NORMAL:		return (nvtt::Quality_Normal);
	case TC_QUALITY_HIGHEST:	return (nvtt::Quality_Highest);
	default:			return (nvtt::Quality_Production);
	}
}

} /* namespace */

extern "C" uint8_t *
compress_bc(const float *rgba, int w, int h, enum tc_bc fmt,
    enum tc_quality quality, size_t *out_len)
{
	/*
	 * NVTT's raw entry point wants the channels in planes, not
	 * interleaved -- ColorBlock::init indexes data[i + c * w * h] -- so
	 * the image is transposed into that shape here rather than a second
	 * copy of it being kept in that layout everywhere else.
	 */
	size_t pixels = (size_t)w * (size_t)h;
	float *planar = (float *)malloc(pixels * 4 * sizeof(float));
	uint8_t *out;

	if (planar == NULL)
		return (NULL);
	for (size_t i = 0; i < pixels; i++) {
		planar[i] = rgba[i * 4 + 0];
		planar[pixels + i] = rgba[i * 4 + 1];
		planar[pixels * 2 + i] = rgba[i * 4 + 2];
		planar[pixels * 3 + i] = rgba[i * 4 + 3];
	}

	{
		nvtt::CompressionOptions co;
		nvtt::OutputOptions oo;
		nvtt::Context ctx;
		collector sink;

		co.setFormat(nvtt_format(fmt));
		co.setQuality(nvtt_quality(quality));
		/*
		 * BC6 is one NVTT format under two names.  Which one it
		 * writes is decided by the pixel type, not the format:
		 * CompressorBC6::compressBlock reads it, and PixelType_Float
		 * is the signed variant -- there is no SignedFloat.
		 */
		if (fmt == TC_BC6S)
			co.setPixelType(nvtt::PixelType_Float);
		else if (fmt == TC_BC6U)
			co.setPixelType(nvtt::PixelType_UnsignedFloat);

		oo.setOutputHeader(false);
		oo.setOutputHandler(&sink);

		if (!ctx.compress(w, h, 1, 0, 0, planar, co, oo) ||
		    sink.failed) {
			free(planar);
			return (NULL);
		}
		free(planar);
		out = sink.buf;
		*out_len = sink.len;
		sink.buf = NULL;	/* handed to the caller */
	}
	return (out);
}
