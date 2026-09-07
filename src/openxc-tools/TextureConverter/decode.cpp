/*
 * decode.cpp -- turning compressed blocks back into pixels.
 *
 * The same libraries that write the blocks read them: astcenc for ASTC, and
 * NVTT's Surface::setImage2D for everything else, which covers BC1 through
 * BC7 and the ETC family in one call.  etc2comp has no decoder at all,
 * which is why Apple's --decompressor list names ETC2COMP nowhere.
 *
 * Everything comes out as tightly packed RGBA float, the shape the rest of
 * this tool passes images around in.  What gets written from it -- R8, RG8,
 * RGB8, RGBA8 or RGBA32 -- is decided by the caller from the source format.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <astcenc.h>
#include <nvtt/nvtt.h>
#include <bc6h/zoh_utils.h>

#include <stdlib.h>
#include <string.h>

#include "decode.h"

namespace {

nvtt::Format
nvtt_decode_format(enum tc_decode f)
{
	switch (f) {
	case TC_DEC_BC1:	return (nvtt::Format_BC1);
	case TC_DEC_BC2:	return (nvtt::Format_BC2);
	case TC_DEC_BC3:	return (nvtt::Format_BC3);
	case TC_DEC_BC4:	return (nvtt::Format_BC4);
	case TC_DEC_BC5:	return (nvtt::Format_BC5);
	case TC_DEC_BC6:
	case TC_DEC_BC6S:	return (nvtt::Format_BC6);
	case TC_DEC_BC7:	return (nvtt::Format_BC7);
	case TC_DEC_ETC2_R:	return (nvtt::Format_ETC2_R);
	case TC_DEC_ETC2_RG:	return (nvtt::Format_ETC2_RG);
	case TC_DEC_ETC2_RGB:	return (nvtt::Format_ETC2_RGB);
	default:		return (nvtt::Format_ETC2_RGBA);
	}
}

} /* namespace */

extern "C" float *
decode_astc(const uint8_t *blocks, size_t len, int w, int h, int bx, int by)
{
	astcenc_config cfg;
	astcenc_context *ctx = NULL;
	astcenc_image img;
	float *out;
	void *slice;
	static const astcenc_swizzle sw = {
		ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A
	};

	if (astcenc_config_init(ASTCENC_PRF_LDR, (unsigned)bx, (unsigned)by, 1,
	    ASTCENC_PRE_MEDIUM, ASTCENC_FLG_DECOMPRESS_ONLY, &cfg) !=
	    ASTCENC_SUCCESS)
		return (NULL);
	if (astcenc_context_alloc(&cfg, 1, &ctx, NULL) != ASTCENC_SUCCESS)
		return (NULL);

	if ((out = (float *)malloc((size_t)w * h * 4 * sizeof(float))) ==
	    NULL) {
		astcenc_context_free(ctx);
		return (NULL);
	}
	slice = out;
	memset(&img, 0, sizeof(img));
	img.dim_x = (unsigned)w;
	img.dim_y = (unsigned)h;
	img.dim_z = 1;
	img.data_type = ASTCENC_TYPE_F32;
	img.data = &slice;

	if (astcenc_decompress_image(ctx, blocks, len, &img, &sw, 0) !=
	    ASTCENC_SUCCESS) {
		free(out);
		astcenc_context_free(ctx);
		return (NULL);
	}
	astcenc_context_free(ctx);
	return (out);
}

extern "C" uint8_t *
decode_astc_u8(const uint8_t *blocks, size_t len, int w, int h, int bx,
    int by)
{
	astcenc_config cfg;
	astcenc_context *ctx = NULL;
	astcenc_image img;
	uint8_t *out;
	void *slice;
	static const astcenc_swizzle sw = {
		ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A
	};

	if (astcenc_config_init(ASTCENC_PRF_LDR, (unsigned)bx, (unsigned)by, 1,
	    ASTCENC_PRE_MEDIUM, ASTCENC_FLG_DECOMPRESS_ONLY, &cfg) !=
	    ASTCENC_SUCCESS)
		return (NULL);
	if (astcenc_context_alloc(&cfg, 1, &ctx, NULL) != ASTCENC_SUCCESS)
		return (NULL);

	if ((out = (uint8_t *)malloc((size_t)w * h * 4)) == NULL) {
		astcenc_context_free(ctx);
		return (NULL);
	}
	slice = out;
	memset(&img, 0, sizeof(img));
	img.dim_x = (unsigned)w;
	img.dim_y = (unsigned)h;
	img.dim_z = 1;
	img.data_type = ASTCENC_TYPE_U8;
	img.data = &slice;

	if (astcenc_decompress_image(ctx, blocks, len, &img, &sw, 0) !=
	    ASTCENC_SUCCESS) {
		free(out);
		astcenc_context_free(ctx);
		return (NULL);
	}
	astcenc_context_free(ctx);
	return (out);
}

extern "C" float *
decode_blocks(const uint8_t *blocks, size_t len, int w, int h,
    enum tc_decode fmt)
{
	nvtt::Surface surface;
	float *out;
	int x, y, c;

	/*
	 * BC6 is one NVTT format under two names on the way out as well.
	 * Its decoder reads a file-scope global rather than a parameter --
	 * CompressorBC6 sets the same one on the way in -- so it is set
	 * here too, or the signed variant comes back as near-zero.
	 */
	ZOH::Utils::FORMAT = fmt == TC_DEC_BC6S ? ZOH::SIGNED_F16 :
	    ZOH::UNSIGNED_F16;

	(void)len;
	if (!surface.setImage2D(nvtt_decode_format(fmt), nvtt::Decoder_D3D10,
	    w, h, blocks))
		return (NULL);
	if ((out = (float *)malloc((size_t)w * h * 4 * sizeof(float))) == NULL)
		return (NULL);
	/*
	 * A Surface holds its channels in planes; the rest of this tool
	 * wants them interleaved.
	 */
	for (c = 0; c < 4; c++) {
		const float *plane = surface.channel(c);

		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++)
				out[((size_t)y * w + x) * 4 + c] =
				    plane[(size_t)y * w + x];
		}
	}
	return (out);
}
