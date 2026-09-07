/*
 * formats.c -- the pixel formats a Khronos container can name.
 *
 * The ASTC entries run in the order Khronos allocated them, which is also
 * the order TextureConverter lists them in: the LDR block sizes from 4x4 to
 * 12x12, then the sRGB aliases, then the same sizes again for HDR.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "formats.h"

/* The base internal formats, spelled as OpenGL does. */
#define	GL_RED		0x1903
#define	GL_RGB		0x1907
#define	GL_RGBA		0x1908
#define	GL_RG		0x8227

struct entry {
	const char	*name;
	uint32_t	 gl;
	uint32_t	 vk;
	uint32_t	 base;			/* glBaseInternalFormat */
	int		 block_x, block_y;	/* 1x1 when uncompressed */
	uint32_t	 metal;			/* MTLPixelFormat, 0 if none */
	uint32_t	 srgb_gl, srgb_vk;	/* --srgb_format, 0 if none */
};

static const struct entry table[] = {
	/* Uncompressed.  Only the float formats are ever written by the
	 * conversion path, but a container from elsewhere may name others. */
	{ "RGBA32",	0x8814, 109, GL_RGBA, 1, 1, 0, 0, 0 },	/* GL_RGBA32F, VK_..R32G32B32A32_SFLOAT */
	{ "RGB32",	0x8815, 106, GL_RGB,  1, 1, 0, 0, 0 },
	{ "RG32",	0x8230, 103, GL_RG,   1, 1, 0, 0, 0 },
	{ "R32",	0x822E, 100, GL_RED,  1, 1, 0, 0, 0 },
	{ "RGBA16",	0x881A, 97,  GL_RGBA, 1, 1, 0, 0, 0 },
	{ "RGB16",	0x881B, 90,  GL_RGB,  1, 1, 0, 0, 0 },
	{ "RG16",	0x822F, 83,  GL_RG,   1, 1, 0, 0, 0 },
	{ "R16",	0x822D, 76,  GL_RED,  1, 1, 0, 0, 0 },
	{ "RGBA8",	0x8058, 37,  GL_RGBA, 1, 1, 0, 0x8C43, 43 },
	{ "RGB8",	0x8051, 23,  GL_RGB,  1, 1, 0, 0x8C41, 29 },
	{ "RG8",	0x822B, 16,  GL_RG,   1, 1, 0, 0x8FBE, 22 },
	{ "R8",		0x8229, 9,   GL_RED,  1, 1, 0, 0x8FBD, 15 },
	/* Apple write GL_RGBA8 for BGRA8: version 1 has no way to say the
	 * channels are the other way round, so it does not say it. */
	/* And GL_SRGB8_ALPHA8 with --srgb_format, which is RGBA8's sRGB
	 * spelling in both containers -- Apple describe the file as RGBA
	 * and write BGRA bytes into it. */
	{ "BGRA8",	0x8058, 44,  GL_RGBA, 1, 1, 0, 0x8C43, 43 },

	/* ASTC, LDR. */
	{ "ASTC4x4",	0x93B0, 157, GL_RGBA, 4, 4, 204, 0x93D0, 158 },
	{ "ASTC5x4",	0x93B1, 159, GL_RGBA, 5, 4, 205, 0x93D1, 160 },
	{ "ASTC5x5",	0x93B2, 161, GL_RGBA, 5, 5, 206, 0x93D2, 162 },
	{ "ASTC6x5",	0x93B3, 163, GL_RGBA, 6, 5, 207, 0x93D3, 164 },
	{ "ASTC6x6",	0x93B4, 165, GL_RGBA, 6, 6, 208, 0x93D4, 166 },
	{ "ASTC8x5",	0x93B5, 167, GL_RGBA, 8, 5, 210, 0x93D5, 168 },
	{ "ASTC8x6",	0x93B6, 169, GL_RGBA, 8, 6, 211, 0x93D6, 170 },
	{ "ASTC8x8",	0x93B7, 171, GL_RGBA, 8, 8, 212, 0x93D7, 172 },
	{ "ASTC10x5",	0x93B8, 173, GL_RGBA, 10, 5, 213, 0x93D8, 174 },
	{ "ASTC10x6",	0x93B9, 175, GL_RGBA, 10, 6, 214, 0x93D9, 176 },
	{ "ASTC10x8",	0x93BA, 177, GL_RGBA, 10, 8, 215, 0x93DA, 178 },
	{ "ASTC10x10",	0x93BB, 179, GL_RGBA, 10, 10, 216, 0x93DB, 180 },
	{ "ASTC12x10",	0x93BC, 181, GL_RGBA, 12, 10, 217, 0x93DC, 182 },
	{ "ASTC12x12",	0x93BD, 183, GL_RGBA, 12, 12, 218, 0x93DD, 184 },

	/* BC.  BC4 carries one channel and BC5 two, and BC6 is colour with
	 * no alpha, so these are the three that are not GL_RGBA. */
	{ "BC1",	0x83F1, 133, GL_RGBA, 4, 4, 0, 0x8C4D, 134 },
	{ "BC2",	0x83F2, 135, GL_RGBA, 4, 4, 0, 0x8C4E, 136 },
	{ "BC3",	0x83F3, 137, GL_RGBA, 4, 4, 0, 0x8C4F, 138 },
	{ "BC4",	0x8DBB, 139, GL_RED,  4, 4, 0, 0, 0 },
	{ "BC5",	0x8DBD, 141, GL_RG,   4, 4, 0, 0, 0 },
	{ "BC6U",	0x8E8F, 143, GL_RGB,  4, 4, 0, 0, 0 },
	{ "BC6S",	0x8E8E, 144, GL_RGB,  4, 4, 0, 0, 0 },
	{ "BC7",	0x8E8C, 145, GL_RGBA, 4, 4, 0, 0x8E8D, 146 },

	/* ETC2 and EAC, the same way: R11 is one channel, RG11 two, and
	 * ETC2_RGB8 has no alpha. */
	{ "ETC2_RGB8",	0x9274, 147, GL_RGB,  4, 4, 0, 0x9275, 148 },
	{ "ETC2_RGB8A1", 0x9276, 149, GL_RGBA, 4, 4, 0, 0x9277, 150 },
	{ "EAC_RGBA8",	0x9278, 151, GL_RGBA, 4, 4, 0, 0x9279, 152 },
	{ "EAC_R11",	0x9270, 153, GL_RED,  4, 4, 0, 0, 0 },
	{ "EAC_RG11",	0x9272, 155, GL_RG,   4, 4, 0, 0, 0 },

	{ NULL,		0, 0, 0, 0, 0, 0, 0, 0 }
};

const char *
format_name_for_gl(uint32_t gl)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (e->gl == gl)
			return (e->name);
	}
	return (NULL);
}

const char *
format_name_for_vk(uint32_t vk)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (e->vk == vk)
			return (e->name);
	}
	return (NULL);
}

_Bool
format_lookup(const char *name, uint32_t *gl, uint32_t *base, int *block_x,
    int *block_y, uint32_t *metal)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (strcmp(e->name, name) != 0)
			continue;
		*gl = e->gl;
		*base = e->base;
		*block_x = e->block_x;
		*block_y = e->block_y;
		*metal = e->metal;
		return (1);
	}
	return (0);
}

/*
 * The sRGB spelling of a format, which --srgb_format asks for.  BC4, BC5,
 * BC6, EAC_R11 and EAC_RG11 have none -- they carry no colour to encode --
 * and neither do the float and sixteen bit formats.  Apple's tool does not
 * check: given --srgb_format and one of those it dereferences a null and
 * crashes, so this says so instead.
 *
 * The Metal enumerant is not tabulated because the sRGB block formats sit
 * eighteen below the LDR ones with the same hole in the middle: 204 is
 * ASTC4x4 and 186 its sRGB, 209 and 191 are both unused.
 */
_Bool
format_srgb_for(const char *name, uint32_t *gl, uint32_t *vk)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (strcmp(e->name, name) != 0)
			continue;
		if (e->srgb_gl == 0)
			return (0);
		*gl = e->srgb_gl;
		*vk = e->srgb_vk;
		return (1);
	}
	return (0);
}

/*
 * The KTX2 descriptors, measured from Apple's .ktx2 output one format at a
 * time.  The models are the Khronos data format ones -- 128 through 134 for
 * the BC family, 161 for ETC2 and EAC, 162 for ASTC -- and the samples say
 * where each part of a block lives.
 *
 * Two things are not guessable from the format alone.  ETC2_RGB8A1 puts
 * both of its samples at bit offset zero rather than side by side, because
 * its alpha is a bit stolen from the colour block rather than a block of
 * its own.  And BC6, being floating point, bounds its sample with the bit
 * patterns of 1.0f and -1.0f instead of the integer range everything else
 * uses.
 */
#define	FULL	0x00000000, 0xffffffff

static const struct { const char *name; struct format_dfd dfd; } dfds[] = {
	{ "BC1",	{ 128, 1, { { 0, 63, 0x01, FULL } } } },
	{ "BC2",	{ 129, 2, { { 0, 63, 0x0f, FULL },
				    { 64, 63, 0x00, FULL } } } },
	{ "BC3",	{ 130, 2, { { 0, 63, 0x0f, FULL },
				    { 64, 63, 0x00, FULL } } } },
	{ "BC4",	{ 131, 1, { { 0, 63, 0x00, FULL } } } },
	{ "BC5",	{ 132, 2, { { 0, 63, 0x00, FULL },
				    { 64, 63, 0x01, FULL } } } },
	{ "BC6U",	{ 133, 1, { { 0, 127, 0x80, 0x00000000,
				      0x3f800000 } } } },
	{ "BC6S",	{ 133, 1, { { 0, 127, 0xc0, 0xbf800000,
				      0x3f800000 } } } },
	{ "BC7",	{ 134, 1, { { 0, 127, 0x00, FULL } } } },
	{ "ETC2_RGB8",	{ 161, 1, { { 0, 63, 0x02, FULL } } } },
	{ "ETC2_RGB8A1", { 161, 2, { { 0, 63, 0x02, FULL },
				     { 0, 63, 0x0f, FULL } } } },
	{ "EAC_RGBA8",	{ 161, 2, { { 0, 63, 0x0f, FULL },
				    { 64, 63, 0x02, FULL } } } },
	{ "EAC_R11",	{ 161, 1, { { 0, 63, 0x00, FULL } } } },
	{ "EAC_RG11",	{ 161, 2, { { 0, 63, 0x00, FULL },
				    { 64, 63, 0x01, FULL } } } },
	{ NULL,		{ 0, 0, { { 0, 0, 0, 0, 0 } } } }
};

uint32_t
format_vk_for(const char *name)
{
	const struct entry *e;

	for (e = table; e->name != NULL; e++) {
		if (strcmp(e->name, name) == 0)
			return (e->vk);
	}
	return (0);
}

/*
 * --srgb_format sets the transfer function, which names the colour channels
 * only: alpha is never encoded, so its sample gains the linear bit, 0x10.
 * It is the sample rather than the format that carries this, so it holds
 * for a block format's alpha sample as much as for a plain channel.
 */
static void
mark_alpha_linear(struct format_dfd *dfd)
{
	int i;

	for (i = 0; i < dfd->nsamples; i++) {
		if ((dfd->sample[i].channel_type & 0x0f) == 15)
			dfd->sample[i].channel_type |= 0x10;
	}
}

_Bool
format_dfd_for(const char *name, _Bool srgb, struct format_dfd *out)
{
	uint32_t gl, base, metal;
	int bx, by, i;

	/*
	 * The uncompressed formats need no table.  Their descriptor is the
	 * RGBSDA model with one sample per channel, laid end to end, and the
	 * only thing that varies is how many channels there are and whether
	 * the samples are bytes or floats.
	 *
	 * Two details are not the obvious ones: alpha's channel type is 15
	 * rather than 3, and a float sample carries the signed and float
	 * bits (0xc0) and is bounded by the patterns of -1.0f and 1.0f
	 * rather than by its integer range.
	 *
	 */
	if (format_lookup(name, &gl, &base, &bx, &by, &metal) && bx == 1) {
		_Bool flt = format_is_float(name);
		/*
		 * --srgb_format takes the swizzle with it: Apple write
		 * RGBA8's sRGB enumerant for BGRA8 and describe the samples
		 * in RGBA order, with the bytes still BGRA.
		 */
		_Bool bgra = name[0] == 'B' && !srgb;
		int channels = base == GL_RED ? 1 : base == GL_RG ? 2 :
		    base == GL_RGB ? 3 : 4;
		int bits = format_channel_bits(name);

		out->color_model = 1;		/* KHR_DF_MODEL_RGBSDA */
		out->nsamples = channels;
		for (i = 0; i < channels; i++) {
			int ch = bgra && i < 3 ? 2 - i : i;

			out->sample[i].bit_offset = (uint16_t)(bits * i);
			out->sample[i].bit_length = (uint8_t)(bits - 1);
			out->sample[i].channel_type =
			    (uint8_t)((i == 3 ? 15 : ch) | (flt ? 0xc0 : 0));
			out->sample[i].lower = flt ? 0xbf800000 : 0x00000000;
			out->sample[i].upper = flt ? 0x3f800000 : 0x000000ff;
		}
		if (srgb)
			mark_alpha_linear(out);
		return (1);
	}

	/* Every ASTC block size shares one descriptor. */
	if (strncmp(name, "ASTC", 4) == 0) {
		out->color_model = 162;
		out->nsamples = 1;
		out->sample[0].bit_offset = 0;
		out->sample[0].bit_length = 127;
		out->sample[0].channel_type = 0x00;
		out->sample[0].lower = 0x00000000;
		out->sample[0].upper = 0xffffffff;
		return (1);
	}
	for (i = 0; dfds[i].name != NULL; i++) {
		if (strcmp(dfds[i].name, name) == 0) {
			*out = dfds[i].dfd;
			if (srgb)
				mark_alpha_linear(out);
			return (1);
		}
	}
	return (0);
}

int
format_channel_bits(const char *name)
{
	size_t n;

	if (name == NULL)
		return (0);
	n = strlen(name);
	if (n >= 2 && strcmp(name + n - 2, "32") == 0)
		return (32);
	if (n >= 2 && strcmp(name + n - 2, "16") == 0)
		return (16);
	if (n >= 1 && name[n - 1] == '8')
		return (8);
	return (0);
}

_Bool
format_is_float(const char *name)
{
	if (name == NULL)
		return (0);
	return (strcmp(name, "RGBA32") == 0 || strcmp(name, "RGB32") == 0 ||
	    strcmp(name, "RG32") == 0 || strcmp(name, "R32") == 0 ||
	    strcmp(name, "RGBA16") == 0 || strcmp(name, "RGB16") == 0 ||
	    strcmp(name, "RG16") == 0 || strcmp(name, "R16") == 0);
}

/*
 * The names AppleTextureConverter.h gives these formats, which is what the
 * .h output writes rather than an enumerant.  They are not derivable from
 * ours: the suffix follows the samples -- Unorm for bytes, F16 for halves,
 * F32 for floats -- but BC6 folds its signedness into the word, so BC6U is
 * Bc6Uf16 and not Bc6UF16, and ETC2_RGB8A1 keeps its A capital where every
 * other letter after the first is lowered.
 *
 * --srgb_format swaps the Unorm ending for Srgb, which is why the formats
 * that have no sRGB spelling are exactly the ones not ending in Unorm plus
 * BC4, BC5, EAC_R11 and EAC_RG11.
 */
static const struct { const char *name, *atc; } atc_names[] = {
	{ "R8",		"atcFormatR8Unorm" },
	{ "RG8",	"atcFormatRg8Unorm" },
	{ "RGB8",	"atcFormatRgb8Unorm" },
	{ "RGBA8",	"atcFormatRgba8Unorm" },
	{ "BGRA8",	"atcFormatBgra8Unorm" },
	{ "R16",	"atcFormatR16F16" },
	{ "RG16",	"atcFormatRg16F16" },
	{ "RGB16",	"atcFormatRgb16F16" },
	{ "RGBA16",	"atcFormatRgba16F16" },
	{ "R32",	"atcFormatR32F32" },
	{ "RG32",	"atcFormatRg32F32" },
	{ "RGB32",	"atcFormatRgb32F32" },
	{ "RGBA32",	"atcFormatRgba32F32" },
	{ "ASTC4x4",	"atcFormatAstc4x4Unorm" },
	{ "ASTC5x4",	"atcFormatAstc5x4Unorm" },
	{ "ASTC5x5",	"atcFormatAstc5x5Unorm" },
	{ "ASTC6x5",	"atcFormatAstc6x5Unorm" },
	{ "ASTC6x6",	"atcFormatAstc6x6Unorm" },
	{ "ASTC8x5",	"atcFormatAstc8x5Unorm" },
	{ "ASTC8x6",	"atcFormatAstc8x6Unorm" },
	{ "ASTC8x8",	"atcFormatAstc8x8Unorm" },
	{ "ASTC10x5",	"atcFormatAstc10x5Unorm" },
	{ "ASTC10x6",	"atcFormatAstc10x6Unorm" },
	{ "ASTC10x8",	"atcFormatAstc10x8Unorm" },
	{ "ASTC10x10",	"atcFormatAstc10x10Unorm" },
	{ "ASTC12x10",	"atcFormatAstc12x10Unorm" },
	{ "ASTC12x12",	"atcFormatAstc12x12Unorm" },
	{ "BC1",	"atcFormatBc1Unorm" },
	{ "BC2",	"atcFormatBc2Unorm" },
	{ "BC3",	"atcFormatBc3Unorm" },
	{ "BC4",	"atcFormatBc4Unorm" },
	{ "BC5",	"atcFormatBc5Unorm" },
	{ "BC6U",	"atcFormatBc6Uf16" },
	{ "BC6S",	"atcFormatBc6Sf16" },
	{ "BC7",	"atcFormatBc7Unorm" },
	{ "ETC2_RGB8",	"atcFormatEtc2Rgb8Unorm" },
	{ "ETC2_RGB8A1", "atcFormatEtc2Rgb8A1Unorm" },
	{ "EAC_RGBA8",	"atcFormatEacRgba8Unorm" },
	{ "EAC_R11",	"atcFormatEacR11Unorm" },
	{ "EAC_RG11",	"atcFormatEacRg11Unorm" },
	{ NULL,		NULL }
};

const char *
format_atc_for(const char *name, _Bool srgb, char *buf, size_t buflen)
{
	uint32_t gl, vk;
	size_t i, n;

	for (i = 0; atc_names[i].name != NULL; i++) {
		if (strcmp(atc_names[i].name, name) != 0)
			continue;
		if (!srgb)
			return (atc_names[i].atc);
		/*
		 * A format with no sRGB enumerant has no sRGB name either,
		 * and Apple print atcFormatUnknown for it rather than
		 * inventing one.
		 */
		if (!format_srgb_for(name, &gl, &vk))
			return ("atcFormatUnknown");
		n = strlen(atc_names[i].atc);
		if (n < 5 || strcmp(atc_names[i].atc + n - 5, "Unorm") != 0)
			return ("atcFormatUnknown");
		if (buflen < n + 1)
			return ("atcFormatUnknown");
		memcpy(buf, atc_names[i].atc, n - 5);
		memcpy(buf + n - 5, "Srgb", 5);
		return (buf);
	}
	return ("atcFormatUnknown");
}

/*
 * How many channels the .h output says the format carries.  It follows the
 * base internal format everywhere but BC6, which Apple call four channels
 * although its descriptor is colour with no alpha -- and a format with no
 * name carries none, so --srgb_format on one that has no sRGB spelling
 * writes atcFormatUnknown and zero channels together.
 */
int
format_atc_channels(const char *name, _Bool srgb)
{
	uint32_t gl, base, metal;
	char buf[64];
	int bx, by;

	if (strcmp(format_atc_for(name, srgb, buf, sizeof(buf)),
	    "atcFormatUnknown") == 0)
		return (0);
	if (strcmp(name, "BC6U") == 0 || strcmp(name, "BC6S") == 0)
		return (4);
	if (!format_lookup(name, &gl, &base, &bx, &by, &metal))
		return (4);
	return (base == GL_RED ? 1 : base == GL_RG ? 2 :
	    base == GL_RGB ? 3 : 4);
}

/*
 * The DXGI enumerant a DDS file names the format with, and the one
 * --srgb_format asks for.  Only the formats Direct3D has a name for are
 * here, which is why the tool writes no DDS for RG8, RGB8, the sixteen and
 * thirty-two bit formats other than the four channel ones, or any of ETC2
 * and EAC.  R8 is the odd one: it has an OpenGL sRGB spelling but no DXGI
 * one, so --srgb_format takes it out of the list.
 *
 * The ASTC values are the ones Microsoft reserved and never shipped: 134
 * for 4x4 and four apart from there, sRGB one above each.
 */
static const struct { const char *name; uint32_t dxgi, srgb; } dxgis[] = {
	{ "RGBA32",	2,   0 },
	{ "RGBA16",	10,  0 },
	{ "R32",	41,  0 },
	{ "RGBA8",	28,  29 },
	{ "BGRA8",	87,  91 },
	{ "R8",		61,  0 },
	{ "ASTC4x4",	134, 135 },
	{ "ASTC5x4",	138, 139 },
	{ "ASTC5x5",	142, 143 },
	{ "ASTC6x5",	146, 147 },
	{ "ASTC6x6",	150, 151 },
	{ "ASTC8x5",	154, 155 },
	{ "ASTC8x6",	158, 159 },
	{ "ASTC8x8",	162, 163 },
	{ "ASTC10x5",	166, 167 },
	{ "ASTC10x6",	170, 171 },
	{ "ASTC10x8",	174, 175 },
	{ "ASTC10x10",	178, 179 },
	{ "ASTC12x10",	182, 183 },
	{ "ASTC12x12",	186, 187 },
	{ "BC1",	71,  72 },
	{ "BC2",	74,  75 },
	{ "BC3",	77,  78 },
	{ "BC4",	80,  0 },
	{ "BC5",	83,  0 },
	{ "BC6U",	95,  0 },
	{ "BC6S",	96,  0 },
	{ "BC7",	98,  99 },
	{ NULL,		0,   0 }
};

uint32_t
format_dxgi_for(const char *name, _Bool srgb)
{
	size_t i;

	for (i = 0; dxgis[i].name != NULL; i++) {
		if (strcmp(dxgis[i].name, name) == 0)
			return (srgb ? dxgis[i].srgb : dxgis[i].dxgi);
	}
	return (0);
}
