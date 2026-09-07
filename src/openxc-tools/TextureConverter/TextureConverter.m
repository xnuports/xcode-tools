/*
 * TextureConverter -- convert, compress and inspect textures.
 *
 * Apple's is seven megabytes, almost all of which is other people's
 * compressors: it names ARM, ETC2COMP, ISPC, NVTT, PVRTC and STB as
 * selectable back ends.  Those are ports here (src/extras, wired in
 * mk/port.d), and what this file is, is the tool around them -- the option
 * parsing, the containers, and the modes.
 *
 * It reads images through ImageIO and works in floating point, which is what
 * Apple's does: converting a PNG with no other options asked writes RGBA32,
 * whatever --format says, and generates the mip chain.
 *
 * One difference is inherent.  Apple stamp a version into every file they
 * write and into their usage banner.  TC_Version is the one a reader checks
 * against -- --check_details compares it -- so it stays theirs, being the
 * version of the format and the options rather than of the binary; KTXwriter
 * says who actually wrote the file, and that is us, so an annotated file is
 * a dozen bytes longer than Apple's and never byte-identical.
 * --disable_annotation turns the whole block off, and then the files match.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#import <Foundation/Foundation.h>

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats.h"
#include "compress.h"
#include "decode.h"
#include "ktx.h"
#include "ktx2.h"
#include "header.h"
#include "dds.h"
#include "mipmap.h"
#include "gamma.h"
#include "usage.h"

/* Our own build, not Apple's; see the note at the top of the file. */
#define TC_VERSION	"4.0.200600"

static const char *progpath;

/* Defined below, beside the compression path they mostly describe. */
/*
 * --flip_x and --flip_y, in place.  --flip_z is accepted and does nothing:
 * there is no third axis in a two dimensional image, and Apple's tool
 * writes the same texels with it as without.
 */
static void
flip_image(float *rgba, int w, int h, bool flip_x, bool flip_y)
{
	int x, y, c;

	if (flip_x) {
		for (y = 0; y < h; y++) {
			for (x = 0; x < w / 2; x++) {
				float *a = rgba + ((size_t)y * w + x) * 4;
				float *b = rgba +
				    ((size_t)y * w + (w - 1 - x)) * 4;

				for (c = 0; c < 4; c++) {
					float t = a[c];

					a[c] = b[c];
					b[c] = t;
				}
			}
		}
	}
	if (flip_y) {
		for (y = 0; y < h / 2; y++) {
			for (x = 0; x < w; x++) {
				float *a = rgba + ((size_t)y * w + x) * 4;
				float *b = rgba +
				    ((size_t)(h - 1 - y) * w + x) * 4;

				for (c = 0; c < 4; c++) {
					float t = a[c];

					a[c] = b[c];
					b[c] = t;
				}
			}
		}
	}
}

/*
 * --max_extent, which is not a resize: Apple halve the image with the
 * mipmap filter, over and over, until neither side is longer than the
 * extent.  The result is bit for bit the level of the mip chain that
 * halving that many times would have reached, and --resize_filter does not
 * touch it -- that option and --resize_round_mode are recorded in
 * TC_Options and change nothing else, in their tool as in this one.
 */
static float *
fit_extent(float *rgba, int *w, int *h, int extent, enum mip_filter which,
    enum mip_wrap wrap)
{
	while (extent > 0 && (*w > extent || *h > extent)) {
		int nw, nh;
		float *half = mip_downsample(rgba, *w, *h, which, wrap,
		    &nw, &nh);

		if (half == NULL)
			break;
		free(rgba);
		rgba = half;
		*w = nw;
		*h = nh;
	}
	return (rgba);
}

static NSString *tc_options_string(NSDictionary<NSString *, NSString *> *,
    NSString *compressor, NSString *fmt);
static bool wants_ktx2(NSString *output);
static NSData *write_ktx2_generic(void **levels, const size_t *sizes,
    const int *widths, const int *heights, int nlevels, const char *name,
    bool premultiplied, bool srgb, NSString *options, bool annotate);
static void *pack_raw(const float *rgba, int w, int h, const char *name,
    size_t *out_len);
static bool wants_header(NSString *output);
static bool wants_dds(NSString *output);
static NSData *write_dds_generic(void **levels, const size_t *sizes,
    const int *widths, const int *heights, int nlevels, const char *name,
    bool srgb);
static NSData *write_header_generic(void **levels, const size_t *sizes,
    const int *widths, const int *heights, int nlevels, const char *name,
    bool srgb, NSString *output,
    NSDictionary<NSString *, NSString *> *opts);

/*
 * Every option the tool takes, with the default the usage text advertises.
 * Kept as one table so that parsing, defaulting and the "did not recognize"
 * message all agree about what exists.
 */
struct option_def {
	const char	*name;
	bool		 takes_value;
	const char	*deflt;
};

static const struct option_def options[] = {
	{ "ASTC_HDR",			false, NULL },
	{ "alpha_mode",			true,  "Ignore" },
	{ "alpha_reference",		true,  "0.950000" },
	{ "alpha_to_coverage",		false, NULL },
	{ "alpha_weight",		false, NULL },
	{ "build_array",		false, NULL },
	{ "build_cubemap",		false, NULL },
	{ "build_mips",			false, NULL },
	{ "build_volume",		false, NULL },
	{ "channel_weighting",		true,  "Perceptual" },
	{ "check_date",			false, NULL },
	{ "check_details",		false, NULL },
	{ "compare",			true,  "" },
	{ "compression_format",		true,  "None" },
	{ "compression_quality",	true,  "Production" },
	{ "compressor",			true,  "Auto" },
	{ "crop_uniform_content",	false, NULL },
	{ "decompressed",		true,  "" },
	{ "decompression_format",	true,  "Auto" },
	{ "decompressor",		true,  "Auto" },
	{ "disable_annotation",		false, NULL },
	{ "disable_multithreading",	false, NULL },
	{ "file_format",		true,  "Auto" },
	{ "flip_x",			false, NULL },
	{ "flip_y",			false, NULL },
	{ "flip_z",			false, NULL },
	{ "format",			true,  "None" },
	{ "gamma_in",			true,  "1.000000" },
	{ "gamma_out",			true,  "1.000000" },
	{ "gamut_in",			true,  "" },
	{ "gamut_out",			true,  "" },
	{ "max_extent",			true,  "16384" },
	{ "max_mipmaps",		true,  "15" },
	{ "metrics",			false, NULL },
	{ "mipmap_filter",		true,  "Kaiser" },
	{ "mode",			true,  "Compress" },
	{ "normal_map",			false, NULL },
	{ "output",			true,  "" },
	{ "progressbar",		false, NULL },
	{ "pvrtc_punch_through",	true,  "Unused" },
	{ "resize_filter",		true,  "Mitchell" },
	{ "resize_round_mode",		true,  "None" },
	{ "rgbm_encoding",		false, NULL },
	{ "rgbm_range",			true,  "6.000000" },
	{ "scale_range",		false, NULL },
	{ "srgb_format",		false, NULL },
	{ "time",			false, NULL },
	{ "verbose",			false, NULL },
	{ "wrap_mode",			true,  "Mirror" },
	{ NULL,				false, NULL }
};

static const struct option_def *
find_option(const char *name, size_t len)
{
	const struct option_def *o;

	for (o = options; o->name != NULL; o++) {
		if (strlen(o->name) == len && strncmp(o->name, name, len) == 0)
			return (o);
	}
	return (NULL);
}

/*
 * The short banner, which is what the tool answers with when it cannot get
 * as far as doing anything.
 */
static void
short_usage(void)
{
	printf("TextureConverter\n");
	printf("Usage options:\n");
	printf("       TextureConverter --h : Detailed Usage\n");
	printf("       TextureConverter -h  : TextureTool Compatibility Mode "
	    "Usage\n");
}

static void
full_usage(void)
{
	const char *const *l;

	printf("\n");
	printf("TextureConverter %s\n", TC_VERSION);
	printf("Usage: %s [Options] Arguments...\n", progpath);
	for (l = tc_usage; *l != NULL; l++)
		printf("%s\n", *l);
}

static void
compat_usage(void)
{
	printf("TextureConverter TextureTool Compatibility Mode");
	printf("Usage: TextureConverter [-hl]\n");
	printf("       TextureConverter [-m] [-e <encoder>] -o <output> "
	    "[-f <format>] <input_image>\n");
	printf("\n");
	printf("       first form:\n");
	printf("         -h       Display this help menu.\n");
	printf("         -l       List available encoders, individual encoder "
	    "options, and file formats.\n");
	printf("\n");
	printf("       second form:\n");
	printf("         -m       Generate a complete mipmap chain from the "
	    "input image.\n");
	printf("         -e       Encode texture levels with <encoder>.\n");
	printf("         -o       Write processed image to <output>.\n");
	printf("         -f       Set file <format> for <output> image.\n");
	printf("\n");
	printf("For detailed usage: TextureConverter --h\n");
}

/* ------------------------------------------------------------------ */
/* Examine.                                                            */
/* ------------------------------------------------------------------ */

/*
 * What the tool knows about an input, however it was stored: a size, a
 * number of mip levels, a format name, and -- for a container it wrote
 * itself -- the stamp saying which build wrote it and with what.
 */
struct texinfo {
	uint32_t	 width, height, levels;
	const char	*format;
	const char	*colorspace;
	const char	*tc_version;
	const char	*tc_options;
};

static bool
examine_ktx(NSData *data, struct ktx *k, struct texinfo *out)
{
	if (!ktx_parse([data bytes], [data length], k))
		return (false);

	out->width = k->width;
	out->height = k->height;
	out->levels = k->levels != 0 ? k->levels : 1;
	out->format = k->version == 1 ?
	    format_name_for_gl(k->gl_internal_format) :
	    format_name_for_vk(k->vk_format);
	/*
	 * Version 1 says nothing about colour, so it is reported unknown.
	 * Version 2 carries a descriptor, and a float format read through it
	 * is extended-range linear.
	 */
	out->colorspace = "unknown";
	if (k->version == 2 && format_is_float(out->format))
		out->colorspace = "ExtendedLinearSRGB";
	out->tc_version = ktx_value(k, "TC_Version");
	out->tc_options = ktx_value(k, "TC_Options");
	return (true);
}

static bool
examine_image(NSURL *url, struct texinfo *out)
{
	CGImageSourceRef src;
	CGImageRef img;

	src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
	if (src == NULL)
		return (false);
	img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
	CFRelease(src);
	if (img == NULL)
		return (false);

	out->width = (uint32_t)CGImageGetWidth(img);
	out->height = (uint32_t)CGImageGetHeight(img);
	out->levels = 1;
	/*
	 * Everything is read into floating point, so an image that arrived
	 * as eight bits per channel still reports as RGBA32.
	 */
	out->format = "RGBA32";
	out->colorspace = "unknown";
	CGImageRelease(img);
	return (true);
}

static int
do_examine(NSString *path)
{
	NSData *data = [NSData dataWithContentsOfFile:path];
	struct texinfo info;
	struct ktx k;
	bool is_ktx = false;

	memset(&info, 0, sizeof(info));
	memset(&k, 0, sizeof(k));

	printf("Examining %s\n\n", [path UTF8String]);

	if (data != nil)
		is_ktx = examine_ktx(data, &k, &info);
	if (!is_ktx && !examine_image([NSURL fileURLWithPath:path], &info)) {
		printf("Error: Could not read input file!\n");
		return (255);
	}

	printf("Size:   %ux%u, %u mip-levels\n", info.width, info.height,
	    info.levels);
	printf("Format: %s\n", info.format != NULL ? info.format : "unknown");
	printf("Colorspace: %s\n", info.colorspace);
	if (info.tc_version != NULL) {
		printf("\nCompressed with TextureConverter: %s\n",
		    info.tc_version);
		printf("Compression options: \n");
		/*
		 * The options are stored as one space separated run and
		 * printed back one to a line, each indented by a tab.  An
		 * empty run still prints its tab, which is what Apple's does.
		 */
		{
			const char *p = info.tc_options != NULL ?
			    info.tc_options : "";

			printf("\t");
			for (; *p != '\0'; p++) {
				if (*p == ' ')
					printf("\n\t");
				else
					putchar(*p);
			}
			printf("\n");
		}
	}
	if (is_ktx)
		ktx_free(&k);
	return (0);
}

/* ------------------------------------------------------------------ */
/* Convert.                                                            */
/* ------------------------------------------------------------------ */

/* --alpha_mode. */
enum alpha_mode {
	ALPHA_IGNORE,		/* the default: the image is treated as opaque */
	ALPHA_PRESERVE,
	ALPHA_PREMULTIPLY
};

/*
 * Read an image into tightly packed RGBA floats, top row first.  The values
 * are the stored samples over 255 with no gamma applied, which is what
 * Apple's writes: a level of theirs holds 0.501961 where the source byte
 * was 128.
 *
 * The samples are taken from the image's own data provider rather than by
 * drawing it into a bitmap context.  A context can only be asked for
 * premultiplied alpha, and dividing that back out does not give the
 * original: a pixel stored (237, 191, 136, 70) comes back (236, 189, 134),
 * where Apple's tool writes 237, 191 and 136.  The provider hands over what
 * was decoded, which is what they read.
 */
static float *
load_rgba(NSString *path, enum alpha_mode amode, int *wp, int *hp)
{
	const float inv255 = 1.0f / 255.0f;
	CGImageSourceRef src;
	CGImageRef img;
	CFDataRef pixels;
	const uint8_t *bytes;
	float *out;
	size_t w, h, bpc, bpp, stride, x, y;
	CGImageAlphaInfo alpha;
	bool has_alpha, alpha_first, premultiplied;

	src = CGImageSourceCreateWithURL((__bridge CFURLRef)
	    [NSURL fileURLWithPath:path], NULL);
	if (src == NULL)
		return (NULL);
	img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
	CFRelease(src);
	if (img == NULL)
		return (NULL);

	w = CGImageGetWidth(img);
	h = CGImageGetHeight(img);
	bpc = CGImageGetBitsPerComponent(img);
	bpp = CGImageGetBitsPerPixel(img);
	stride = CGImageGetBytesPerRow(img);
	alpha = CGImageGetAlphaInfo(img);

	/*
	 * Only the eight bit layouts are read directly; anything else --
	 * sixteen bit, floating point, indexed, CMYK -- would need the
	 * conversion a bitmap context does, and is not handled yet.
	 */
	if (w == 0 || h == 0 || bpc != 8 || (bpp != 24 && bpp != 32) ||
	    (CGImageGetBitmapInfo(img) & kCGBitmapByteOrderMask) ==
	    kCGBitmapByteOrder32Little) {
		CGImageRelease(img);
		return (NULL);
	}
	switch (alpha) {
	case kCGImageAlphaNone:
	case kCGImageAlphaNoneSkipLast:
		has_alpha = false;
		alpha_first = false;
		premultiplied = false;
		break;
	case kCGImageAlphaNoneSkipFirst:
		has_alpha = false;
		alpha_first = true;
		premultiplied = false;
		break;
	case kCGImageAlphaLast:
		has_alpha = true;
		alpha_first = false;
		premultiplied = false;
		break;
	case kCGImageAlphaFirst:
		has_alpha = true;
		alpha_first = true;
		premultiplied = false;
		break;
	case kCGImageAlphaPremultipliedLast:
		has_alpha = true;
		alpha_first = false;
		premultiplied = true;
		break;
	case kCGImageAlphaPremultipliedFirst:
		has_alpha = true;
		alpha_first = true;
		premultiplied = true;
		break;
	default:
		CGImageRelease(img);
		return (NULL);
	}

	pixels = CGDataProviderCopyData(CGImageGetDataProvider(img));
	CGImageRelease(img);
	if (pixels == NULL)
		return (NULL);
	bytes = CFDataGetBytePtr(pixels);
	if ((size_t)CFDataGetLength(pixels) < stride * h) {
		CFRelease(pixels);
		return (NULL);
	}

	if ((out = malloc(w * h * 4 * sizeof(*out))) == NULL) {
		CFRelease(pixels);
		return (NULL);
	}
	for (y = 0; y < h; y++) {
		const uint8_t *row = bytes + y * stride;

		for (x = 0; x < w; x++) {
			const uint8_t *p = row + x * (bpp / 8);
			float *o = out + (y * w + x) * 4;
			unsigned r, g, b, a;

			if (alpha_first) {
				a = p[0];
				r = p[1];
				g = p[2];
				b = p[3];
			} else {
				r = p[0];
				g = p[1];
				b = p[2];
				a = bpp == 32 ? p[3] : 255;
			}
			if (!has_alpha)
				a = 255;
			if (premultiplied && a != 0 && a != 255) {
				r = r * 255u / a;
				g = g * 255u / a;
				b = b * 255u / a;
			}

			/*
			 * Ignore, the default, treats the image as opaque
			 * rather than dropping the colour behind it; the
			 * other two keep the alpha, one of them folding it
			 * into the colour first.
			 */
			if (amode == ALPHA_IGNORE)
				a = 255;
			o[0] = r * inv255;
			o[1] = g * inv255;
			o[2] = b * inv255;
			o[3] = a * inv255;
		}
	}
	CFRelease(pixels);
	*wp = (int)w;
	*hp = (int)h;
	return (out);
}

/*
 * Fold alpha into colour.  Only the base level: the mip chain is built from
 * the straight colour and left alone, which is what Apple's does -- their
 * --alpha_mode=Premultiply and --alpha_mode=Preserve write byte for byte the
 * same second level, and only the first differs.
 */
static void
premultiply_base(float *rgba, int w, int h)
{
	size_t i, n = (size_t)w * h * 4;

	for (i = 0; i < n; i += 4) {
		rgba[i + 0] *= rgba[i + 3];
		rgba[i + 1] *= rgba[i + 3];
		rgba[i + 2] *= rgba[i + 3];
	}
}

/*
 * A normal map's colour is a direction, not a colour, and filtering it does
 * not keep it one: averaging two unit vectors gives a shorter one.  So the
 * texels are expanded out of [0,1] into [-1,1], normalised, and packed back,
 * and that happens to the base level and again after every downsample --
 * Apple's chain has unit length at every level, which a single pass at the
 * top would not give.
 */
static void
normalize_normals(float *rgba, int w, int h)
{
	size_t i, n = (size_t)w * h * 4;

	/*
	 * The three steps are NVTT's, in NVTT's order, because the answer
	 * has to agree to the last bit: FloatImage::expandNormals is
	 * scaleBias(2, -1), normalize scales by the reciprocal of the
	 * length rather than dividing, and packNormals is scaleBias(0.5,
	 * 0.5).  Dividing instead of multiplying by the reciprocal moves a
	 * few hundred samples per level by one unit in the last place.
	 */
	for (i = 0; i < n; i += 4) {
		float x = rgba[i + 0] * 2.0f + -1.0f;
		float y = rgba[i + 1] * 2.0f + -1.0f;
		float z = rgba[i + 2] * 2.0f + -1.0f;
		float len = sqrtf(x * x + y * y + z * z);

		if (len != 0.0f) {
			float rcp = 1.0f / len;

			x *= rcp;
			y *= rcp;
			z *= rcp;
		}
		rgba[i + 0] = x * 0.5f + 0.5f;
		rgba[i + 1] = y * 0.5f + 0.5f;
		rgba[i + 2] = z * 0.5f + 0.5f;
	}
}

static enum alpha_mode
alpha_mode_of(NSDictionary<NSString *, NSString *> *opts)
{
	NSString *m = opts[@"alpha_mode"];

	if ([m caseInsensitiveCompare:@"Preserve"] == NSOrderedSame)
		return (ALPHA_PRESERVE);
	if ([m caseInsensitiveCompare:@"Premultiply"] == NSOrderedSame)
		return (ALPHA_PREMULTIPLY);
	/*
	 * --normal_map keeps the alpha channel whatever --alpha_mode says,
	 * which is visible in Apple's output: with it, the alpha of a
	 * converted image is the source's rather than the solid 1.0 that
	 * Ignore writes.
	 */
	if (opts[@"normal_map"] != nil)
		return (ALPHA_PRESERVE);
	return (ALPHA_IGNORE);
}

static void
put32(NSMutableData *d, uint32_t v)
{
	uint8_t b[4] = {
		(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
		(uint8_t)(v >> 24)
	};

	[d appendBytes:b length:sizeof(b)];
}

/*
 * One key/value pair: a length, the NUL terminated key, the value, then
 * padding to a four byte boundary.  The padding is not counted in the
 * length but is counted in the header's bytesOfKeyValueData, which is the
 * kind of asymmetry that costs an afternoon if the two are written
 * separately -- so everything goes through here.
 */
static void
put_kv_bytes(NSMutableData *d, const char *key, const void *value, size_t vlen)
{
	size_t klen = strlen(key) + 1;
	static const uint8_t pad[4] = { 0, 0, 0, 0 };

	put32(d, (uint32_t)(klen + vlen));
	[d appendBytes:key length:klen];
	[d appendBytes:value length:vlen];
	[d appendBytes:pad length:(4 - (klen + vlen) % 4) % 4];
}

static void
put_kv(NSMutableData *d, const char *key, const char *value)
{
	put_kv_bytes(d, key, value, strlen(value) + 1);
}

/* The two OpenGL types this tool ever writes. */
#define	GL_UNSIGNED_BYTE	0x1401
#define	GL_FLOAT		0x1406
#define	GL_HALF_FLOAT		0x140B

static const uint8_t ktx1_id[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

/*
 * KTX version 1.  The conversion path writes RGBA32F whatever --format asks
 * for, which is what Apple's does; the compression path writes whichever
 * block format was asked for, and then the samples are the encoder's.
 */
static NSData *
write_ktx_generic(void **levels, const size_t *sizes, const int *widths,
    const int *heights, int nlevels, uint32_t gl_internal, uint32_t gl_base,
    uint32_t gl_type, uint32_t gl_type_size, uint32_t gl_format,
    uint32_t texel_bytes, uint32_t metal, bool premultiplied,
    NSString *options, bool annotate)
{
	NSMutableData *out = [NSMutableData data];
	NSMutableData *kvd = [NSMutableData data];
	int i;

	if (annotate) {
		put_kv(kvd, "TC_Version", TC_VERSION);
		put_kv(kvd, "KTXwriter", "Apple TextureConverter " TC_VERSION);
		/*
		 * TC_Options records what the compressor was asked for, and
		 * the conversion path asks a compressor nothing.  Apple omit
		 * the key entirely there rather than writing it empty.
		 */
		if (options.length != 0)
			put_kv(kvd, "TC_Options", [options UTF8String]);
	}
	/*
	 * The Metal format is not an annotation either.  Only the ASTC
	 * formats carry one; Apple record none for the BC or ETC families.
	 */
	if (metal != 0) {
		uint8_t v[4] = {
			(uint8_t)metal, (uint8_t)(metal >> 8),
			(uint8_t)(metal >> 16), (uint8_t)(metal >> 24)
		};

		put_kv_bytes(kvd, "KTXmetalPixelFormat", v, sizeof(v));
	}
	/*
	 * A reader has to be told the colour has alpha folded into it, so
	 * this is part of describing the file rather than an annotation and
	 * --disable_annotation leaves it alone.  Written only when the
	 * colour actually was premultiplied.
	 */
	if (premultiplied) {
		static const uint8_t one[4] = { 1, 0, 0, 0 };

		put_kv_bytes(kvd, "com.apple.image.premultipliedAlpha", one,
		    sizeof(one));
	}

	[out appendBytes:ktx1_id length:sizeof(ktx1_id)];
	put32(out, 0x04030201);		/* endianness */
	/*
	 * A compressed format names no type and no size -- the block layout
	 * is the internal format's business -- so those three come in as
	 * zero, one and zero for it.  An uncompressed one names all three.
	 */
	put32(out, gl_type);
	put32(out, gl_type_size);
	put32(out, gl_format);
	put32(out, gl_internal);
	/*
	 * The base format says how many channels the internal one carries,
	 * so it is not always GL_RGBA: BC4 and EAC_R11 are GL_RED, BC5 and
	 * EAC_RG11 GL_RG, and BC6 and ETC2_RGB8 GL_RGB.  formats.c holds
	 * the mapping.
	 */
	put32(out, gl_base);
	put32(out, (uint32_t)widths[0]);
	put32(out, (uint32_t)heights[0]);
	put32(out, 0);			/* pixelDepth */
	put32(out, 0);			/* numberOfArrayElements */
	put32(out, 1);			/* numberOfFaces */
	put32(out, (uint32_t)nlevels);
	put32(out, (uint32_t)[kvd length]);
	[out appendData:kvd];

	/*
	 * Version 1 pads each row of an uncompressed level to four bytes and
	 * counts the padding in imageSize; version 2 packs the rows tight,
	 * so the packers hand over tight rows and the padding is put in
	 * here.  A block format has no rows to pad -- texel_bytes is zero
	 * for it -- and only its level is rounded up.
	 */
	for (i = 0; i < nlevels; i++) {
		static const uint8_t pad[4] = { 0, 0, 0, 0 };
		size_t row = texel_bytes == 0 ? sizes[i] :
		    (size_t)widths[i] * texel_bytes;
		size_t stride = (row + 3) & ~(size_t)3;
		size_t total = texel_bytes == 0 ? sizes[i] :
		    stride * (size_t)heights[i];
		size_t y;

		put32(out, (uint32_t)total);
		if (row == stride) {
			[out appendBytes:levels[i] length:sizes[i]];
		} else {
			for (y = 0; y < (size_t)heights[i]; y++) {
				[out appendBytes:(const uint8_t *)levels[i] +
				    y * row length:row];
				[out appendBytes:pad length:stride - row];
			}
		}
		/* Each level is padded to a four byte boundary. */
		[out appendBytes:pad length:(4 - total % 4) % 4];
	}
	return (out);
}

static int
do_convert(NSString *path, NSDictionary<NSString *, NSString *> *opts)
{
	enum { MAX_LEVELS = 32 };
	float *levels[MAX_LEVELS];
	int widths[MAX_LEVELS], heights[MAX_LEVELS];
	NSString *output = opts[@"output"];
	NSString *filter = opts[@"mipmap_filter"];
	enum mip_filter which = MIP_FILTER_KAISER;
	enum mip_wrap wrap = MIP_WRAP_MIRROR;
	bool normal = opts[@"normal_map"] != nil;
	NSData *data;
	int n = 0, i, maxlevels;

	printf("Converting %s\n\n", [path UTF8String]);

	if ([filter caseInsensitiveCompare:@"Box"] == NSOrderedSame)
		which = MIP_FILTER_BOX;
	else if ([filter caseInsensitiveCompare:@"Triangle"] == NSOrderedSame)
		which = MIP_FILTER_TRIANGLE;
	if ([opts[@"wrap_mode"] caseInsensitiveCompare:@"Clamp"] ==
	    NSOrderedSame)
		wrap = MIP_WRAP_CLAMP;
	else if ([opts[@"wrap_mode"] caseInsensitiveCompare:@"Repeat"] ==
	    NSOrderedSame)
		wrap = MIP_WRAP_REPEAT;

	if ((levels[0] = load_rgba(path, alpha_mode_of(opts), &widths[0],
	    &heights[0])) == NULL) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	flip_image(levels[0], widths[0], heights[0],
	    opts[@"flip_x"] != nil, opts[@"flip_y"] != nil);
	/*
	 * --gamma_in takes the colour to linear before anything samples it,
	 * so the chain is filtered in linear space; --gamma_out puts every
	 * level back afterwards, the premultiply included.
	 *
	 * Neither runs for a normal map, for the reason the premultiply
	 * does not: the channels are a direction rather than a colour, and
	 * a transfer function over them means nothing.  Apple write the
	 * same texels with --normal_map --gamma_in=2.2 as with --normal_map
	 * alone, at every level.
	 */
	if (!normal && [opts[@"gamma_in"] floatValue] != 1.0f)
		image_gamma(levels[0], widths[0], heights[0],
		    [opts[@"gamma_in"] floatValue], 1);
	levels[0] = fit_extent(levels[0], &widths[0], &heights[0],
	    [opts[@"max_extent"] intValue], which, wrap);
	if (normal)
		normalize_normals(levels[0], widths[0], heights[0]);
	n = 1;

	maxlevels = [opts[@"max_mipmaps"] intValue];
	if (maxlevels <= 0 || maxlevels > MAX_LEVELS)
		maxlevels = MAX_LEVELS;
	while (n < maxlevels && (widths[n - 1] > 1 || heights[n - 1] > 1)) {
		levels[n] = mip_downsample(levels[n - 1], widths[n - 1],
		    heights[n - 1], which, wrap, &widths[n], &heights[n]);
		if (levels[n] == NULL)
			break;
		if (normal)
			normalize_normals(levels[n], widths[n], heights[n]);
		n++;
	}

	/*
	 * Not for a normal map: its colour is a direction and folding alpha
	 * into it would mean nothing.  Apple's --normal_map
	 * --alpha_mode=Premultiply writes the same texels as --normal_map
	 * alone, so the premultiply is simply not done.
	 */
	if (!normal && alpha_mode_of(opts) == ALPHA_PREMULTIPLY)
		premultiply_base(levels[0], widths[0], heights[0]);

	/*
	 * A gamma of one is no gamma at all, and skipping it is not just an
	 * optimisation: the exponentiation clamps at zero, and the Kaiser
	 * filter undershoots there, so running it would quietly lift every
	 * negative sample the chain produced.
	 */
	if (!normal && [opts[@"gamma_out"] floatValue] != 1.0f) {
		for (i = 0; i < n; i++)
			image_gamma(levels[i], widths[i], heights[i],
			    [opts[@"gamma_out"] floatValue], 0);
	}

	{
		void *ptrs[MAX_LEVELS];
		size_t sizes[MAX_LEVELS];

		for (i = 0; i < n; i++) {
			ptrs[i] = levels[i];
			sizes[i] = (size_t)widths[i] * heights[i] * 4 *
			    sizeof(float);
		}
		NSString *options = tc_options_string(opts, nil, nil);
		bool annotate = opts[@"disable_annotation"] == nil;
		bool prem = alpha_mode_of(opts) == ALPHA_PREMULTIPLY;

		data = wants_dds(output) ?
		    write_dds_generic(ptrs, sizes, widths, heights, n,
		        "RGBA32", false) :
		    wants_header(output) ?
		    write_header_generic(ptrs, sizes, widths, heights, n,
		        "RGBA32", false, output, opts) :
		    wants_ktx2(output) ?
		    write_ktx2_generic(ptrs, sizes, widths, heights, n,
		        "RGBA32", prem, false, options, annotate) :
		    write_ktx_generic(ptrs, sizes, widths, heights, n,
		        0x8814, 0x1908, GL_FLOAT, 4, 0x1908, 16, 0, prem,
		        options, annotate);
	}
	for (i = 0; i < n; i++)
		free(levels[i]);

	/*
	 * A DDS the format has no DXGI name for has already said so, on
	 * stderr, and Apple leave it there: no file, and still a zero exit.
	 */
	if (data == nil)
		return (wants_dds(output) ? 0 : 255);
	if (output.length == 0) {
		printf("Error: Missing output file path!\n");
		return (255);
	}
	if (![data writeToFile:output atomically:NO]) {
		printf("Error: Could not write output file!\n");
		return (255);
	}
	return (0);
}

/*
 * Which container to write.  Apple decide it from the output file's
 * extension and nothing else: --file_format names KTX2 among its choices
 * and does not select it, so a path ending in anything but .ktx2 gets
 * version 1 however the flag is set.
 */
static bool
wants_ktx2(NSString *output)
{
	return ([[output pathExtension] caseInsensitiveCompare:@"ktx2"] ==
	    NSOrderedSame);
}

/* The .h and .dds outputs go the same way: by the extension, not a flag. */
static bool
wants_header(NSString *output)
{
	return ([[output pathExtension] caseInsensitiveCompare:@"h"] ==
	    NSOrderedSame);
}

static bool
wants_dds(NSString *output)
{
	return ([[output pathExtension] caseInsensitiveCompare:@"dds"] ==
	    NSOrderedSame);
}

/*
 * The levels as a DirectDraw surface.  A format Direct3D has no name for
 * cannot be written -- Apple say so on stderr, having already compressed
 * it, and leave no file behind but still exit zero.
 */
static NSData *
write_dds_generic(void **levels, const size_t *sizes, const int *widths,
    const int *heights, int nlevels, const char *name, bool srgb)
{
	uint32_t dxgi = format_dxgi_for(name, srgb);
	char buf[64];
	uint8_t *bytes;
	size_t len;
	NSData *out;

	if (dxgi == 0) {
		fprintf(stderr, "Error: Compression format %s not supported "
		    "for DDS files\n", format_atc_for(name, srgb, buf,
		    sizeof(buf)));
		return (nil);
	}
	bytes = dds_write(levels, sizes, widths[0], heights[0], nlevels,
	    dxgi, &len);
	if (bytes == NULL)
		return (nil);
	out = [NSData dataWithBytes:bytes length:len];
	free(bytes);
	return (out);
}

/*
 * The levels as a C header.  The identifier everything is named after is
 * the output file's stem, and the gamut is whatever --gamut_out asked for
 * -- the one place either of the gamut options leaves a mark.
 */
static NSData *
write_header_generic(void **levels, const size_t *sizes, const int *widths,
    const int *heights, int nlevels, const char *name, bool srgb,
    NSString *output, NSDictionary<NSString *, NSString *> *opts)
{
	NSString *gamut = opts[@"gamut_out"];
	const char *gname = "atcColorGamutUnknown";
	char buf[64];
	char *text;
	size_t len;
	NSData *out;

	if ([gamut caseInsensitiveCompare:@"sRGB"] == NSOrderedSame)
		gname = "atcColorGamutSRGB";
	else if ([gamut caseInsensitiveCompare:@"DisplayP3"] == NSOrderedSame)
		gname = "atcColorGamutDisplayP3";
	text = header_write(levels, sizes, widths, heights, nlevels, name,
	    format_atc_for(name, srgb, buf, sizeof(buf)), gname,
	    [[[output lastPathComponent] stringByDeletingPathExtension]
	    UTF8String], srgb, &len);
	if (text == NULL)
		return (nil);
	out = [NSData dataWithBytes:text length:len];
	free(text);
	return (out);
}

/*
 * The compressed levels as a version 2 container.  nil when the format has
 * no data format descriptor here, which is every uncompressed one.
 */
static NSData *
write_ktx2_generic(void **levels, const size_t *sizes, const int *widths,
    const int *heights, int nlevels, const char *name, bool premultiplied,
    bool srgb, NSString *options, bool annotate)
{
	struct format_dfd dfd;
	uint32_t gl, base, metal, vk = 0, srgb_gl;
	int bx, by, block_bytes, type_size;
	uint8_t *bytes;
	size_t len;
	NSData *out;

	if (!format_dfd_for(name, srgb, &dfd) ||
	    !format_lookup(name, &gl, &base, &bx, &by, &metal))
		return (nil);
	/*
	 * How big one texel block is, which sets the alignment the levels
	 * are padded to.  Counted from the base level rather than taken from
	 * the smallest, which is only one block when the chain runs all the
	 * way down: --max_mipmaps=2 stops it at four.
	 */
	block_bytes = (int)(sizes[0] /
	    ((size_t)((widths[0] + bx - 1) / bx) *
	     (size_t)((heights[0] + by - 1) / by)));
	type_size = bx == 1 ? format_channel_bits(name) / 8 : 1;
	if (!srgb || !format_srgb_for(name, &srgb_gl, &vk))
		vk = format_vk_for(name);
	bytes = ktx2_write(levels, sizes, widths, heights, nlevels,
	    vk, block_bytes, bx, by, type_size, &dfd,
	    premultiplied, srgb,
	    annotate ? "Apple TextureConverter " TC_VERSION " / libktx v4.0" :
	    "Unidentified app / libktx v4.0",
	    /*
	     * An empty options string is no options, and the key is left
	     * out rather than written empty -- the same rule version 1
	     * follows, and the conversion path is where it shows, since it
	     * drives no compressor and so names none.
	     */
	    annotate && options.length != 0 ? [options UTF8String] : NULL,
	    annotate ? TC_VERSION : NULL, &len);
	if (bytes == NULL)
		return (nil);
	out = [NSData dataWithBytes:bytes length:len];
	free(bytes);
	return (out);
}

/* ------------------------------------------------------------------ */
/* Compress.                                                           */
/* ------------------------------------------------------------------ */

/*
 * The TC_Options annotation.
 *
 * Apple record what the texture was actually made with, and they record it
 * their way rather than the caller's: the order is fixed, so giving the
 * same options in a different order writes the same string; only options
 * that differ from their default appear, so spelling out a default writes
 * nothing; a flag is written as "name=true"; and options that cannot change
 * the pixels -- --verbose, --time, --disable_multithreading -- are left out
 * even when given.
 *
 * The order below is theirs, measured by handing their tool every option at
 * once and reading back what it wrote.  --gamut_in and --gamut_out are not
 * in it: their tool accepts both and records neither.
 */
static NSString *
tc_options_string(NSDictionary<NSString *, NSString *> *opts,
    NSString *compressor, NSString *fmt)
{
	static const char *order[] = {
		"compression_quality", "gamma_in", "gamma_out", "srgb_format",
		"max_mipmaps", "mipmap_filter", "alpha_mode",
		"alpha_to_coverage", "alpha_weight", "flip_x", "flip_y",
		"flip_z", "max_extent", "resize_filter", "resize_round_mode",
		"crop_uniform_content", "wrap_mode", "normal_map",
		"rgbm_encoding", "scale_range", "channel_weighting"
	};
	NSMutableString *out = [NSMutableString string];
	size_t i;

	/*
	 * The conversion path names no compressor and no compression format,
	 * because it runs neither; it still records everything else.
	 */
	if (compressor != nil)
		[out appendFormat:@"compressor=%@ compression_format=%@",
		    compressor, fmt];

	for (i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
		NSString *name = [NSString stringWithUTF8String:order[i]];
		NSString *value = opts[name];
		const struct option_def *def;

		if (value == nil)
			continue;
		for (def = options; def->name != NULL; def++) {
			if (strcmp(def->name, order[i]) == 0)
				break;
		}
		if (def->name == NULL)
			continue;
		if (!def->takes_value) {
			[out appendFormat:@"%s%@=true",
			    out.length ? " " : "", name];
			continue;
		}
		/* A value equal to the default is not worth recording. */
		if (def->deflt != NULL && [value isEqualToString:
		    [NSString stringWithUTF8String:def->deflt]])
			continue;
		[out appendFormat:@"%s%@=%@", out.length ? " " : "",
		    name, value];
	}
	return (out);
}

/*
 * The ETC2 and EAC formats, by Apple's names for them.  All five go to the
 * same encoder, and Auto sends them there at every quality.
 */
static bool
etc_format_of(NSString *name, enum tc_etc *out)
{
	static const struct { const char *name; enum tc_etc etc; } etcs[] = {
		{ "ETC2_RGB8", TC_ETC2_RGB8 },
		{ "ETC2_RGB8A1", TC_ETC2_RGB8A1 },
		{ "EAC_RGBA8", TC_EAC_RGBA8 },
		{ "EAC_R11", TC_EAC_R11 },
		{ "EAC_RG11", TC_EAC_RG11 }
	};
	size_t i;

	for (i = 0; i < sizeof(etcs) / sizeof(etcs[0]); i++) {
		if ([name isEqualToString:
		    [NSString stringWithUTF8String:etcs[i].name]]) {
			*out = etcs[i].etc;
			return (true);
		}
	}
	return (false);
}

/*
 * The BC formats, by the names Apple's tool uses for them.  BC6 is one
 * format inside NVTT under two names; the split is Apple's, and theirs is
 * the spelling that has to be accepted.
 */
static bool
bc_format_of(NSString *name, enum tc_bc *out)
{
	static const struct { const char *name; enum tc_bc bc; } bcs[] = {
		{ "BC1", TC_BC1 }, { "BC2", TC_BC2 }, { "BC3", TC_BC3 },
		{ "BC4", TC_BC4 }, { "BC5", TC_BC5 }, { "BC6U", TC_BC6U },
		{ "BC6S", TC_BC6S }, { "BC7", TC_BC7 }
	};
	size_t i;

	for (i = 0; i < sizeof(bcs) / sizeof(bcs[0]); i++) {
		if ([name isEqualToString:
		    [NSString stringWithUTF8String:bcs[i].name]]) {
			*out = bcs[i].bc;
			return (true);
		}
	}
	return (false);
}

/*
 * Build the mip chain the same way the conversion path does, then hand each
 * level to the encoder.
 */
static int
do_compress(NSString *path, NSDictionary<NSString *, NSString *> *opts)
{
	enum { MAX_LEVELS = 32 };
	float *levels[MAX_LEVELS];
	void *blocks[MAX_LEVELS];
	size_t sizes[MAX_LEVELS];
	int widths[MAX_LEVELS], heights[MAX_LEVELS];
	NSString *fmt = opts[@"compression_format"];
	NSString *output = opts[@"output"];
	NSString *filter = opts[@"mipmap_filter"];
	NSString *quality = opts[@"compression_quality"];
	struct tc_astc_options aopt;
	enum mip_filter which = MIP_FILTER_KAISER;
	enum mip_wrap wrap = MIP_WRAP_MIRROR;
	bool normal = opts[@"normal_map"] != nil;
	enum tc_bc bc = TC_BC1;
	enum tc_etc etc = TC_ETC2_RGB8;
	bool srgb = opts[@"srgb_format"] != nil;
	uint32_t type = 0, type_size = 1, gl_format = 0, texel = 0;
	NSString *compressor;
	uint32_t gl, base, metal;
	NSData *data;
	int n = 0, i, maxlevels;

	if (!format_lookup([fmt UTF8String], &gl, &base, &aopt.block_x,
	    &aopt.block_y, &metal)) {
		printf("Error: Unsupported compression format \"%s\"!\n",
		    [fmt UTF8String]);
		short_usage();
		return (255);
	}
	/*
	 * Which back end.  Apple's --compressor defaults to Auto, and Auto
	 * is not a search: ASTC goes to ARM's encoder, every one of BC1
	 * through BC7 to NVTT, and the ETC2 and EAC formats to ETC2COMP.
	 * The name is printed before any work is done, as theirs is.
	 */
	aopt.quality = TC_QUALITY_PRODUCTION;
	if ([quality caseInsensitiveCompare:@"Fastest"] == NSOrderedSame)
		aopt.quality = TC_QUALITY_FASTEST;
	else if ([quality caseInsensitiveCompare:@"Normal"] == NSOrderedSame)
		aopt.quality = TC_QUALITY_NORMAL;
	else if ([quality caseInsensitiveCompare:@"Highest"] == NSOrderedSame)
		aopt.quality = TC_QUALITY_HIGHEST;

	if (aopt.block_x == 1) {
		/*
		 * An uncompressed format runs no encoder.  Apple still name
		 * one, and the name they name is RAW.  Version 1 spells the
		 * samples out rather than calling them a block: the type,
		 * its size, and the channel order.
		 */
		int bits = format_channel_bits([fmt UTF8String]);

		compressor = @"RAW";
		type = bits == 8 ? GL_UNSIGNED_BYTE :
		    bits == 16 ? GL_HALF_FLOAT : GL_FLOAT;
		type_size = (uint32_t)(bits / 8);
		gl_format = base;
		texel = type_size * (uint32_t)(base == 0x1903 ? 1 :
		    base == 0x8227 ? 2 : base == 0x1907 ? 3 : 4);
	} else if ([fmt hasPrefix:@"ASTC"]) {
		compressor = @"ARM";
	} else if (etc_format_of(fmt, &etc)) {
		compressor = @"ETC2COMP";
		if (opts[@"compressor"] != nil &&
		    [opts[@"compressor"] caseInsensitiveCompare:@"Auto"] !=
		    NSOrderedSame)
			compressor = [opts[@"compressor"] uppercaseString];
	} else if (bc_format_of(fmt, &bc)) {
		/*
		 * --compressor defaults to Auto, and Auto is a table rather
		 * than a search.  Every BC format goes to NVTT with two
		 * exceptions, both measured from Apple's tool: BC1 at
		 * Highest goes to STB, and BC7 at Fastest to ISPC.
		 */
		if (bc == TC_BC1 && aopt.quality == TC_QUALITY_HIGHEST)
			compressor = @"STB";
		else if (bc == TC_BC7 && aopt.quality == TC_QUALITY_FASTEST)
			compressor = @"ISPC";
		else
			compressor = @"NVTT";
		/* Naming one overrides the table. */
		if (opts[@"compressor"] != nil &&
		    [opts[@"compressor"] caseInsensitiveCompare:@"Auto"] !=
		    NSOrderedSame)
			compressor = [opts[@"compressor"] uppercaseString];
		/*
		 * BC1 carries one bit of alpha, and NVTT will only spend it
		 * if asked: Format_BC1 writes the opaque variant and
		 * Format_BC1a the punch-through one.  Apple pick the second
		 * whenever the alpha channel is not being ignored, which is
		 * what --alpha_mode says.
		 */
		/*
		 * The alpha mode the caller named, not the one in force:
		 * --normal_map preserves alpha for its own reasons (see
		 * alpha_mode_of) and that must not turn BC1 into the
		 * punch-through variant, which is a different block layout.
		 */
		if (bc == TC_BC1 && opts[@"alpha_mode"] != nil &&
		    [opts[@"alpha_mode"] caseInsensitiveCompare:@"Ignore"] !=
		    NSOrderedSame)
			bc = TC_BC1A;
		/*
		 * NVTT has a normal-map variant of BC3 -- the x and y of the
		 * normal in the two channels a DXT5 stores best -- and
		 * --normal_map is what selects it.
		 */
		if (bc == TC_BC3 && normal)
			bc = TC_BC3N;
		/*
		 * ISPC is the one back end this tree has no port for.  Say
		 * so rather than quietly encoding with a different one: the
		 * blocks would not be Apple's and nothing would say why.
		 */
		if ([compressor isEqualToString:@"ISPC"]) {
			printf("Using Compressor: ISPC\n");
			printf("Error: The ISPC compressor is not available "
			    "in this build!\n");
			return (255);
		}
	} else {
		printf("Error: Compression format \"%s\" is not implemented "
		    "in this build!\n", [fmt UTF8String]);
		return (255);
	}

	aopt.perceptual = [opts[@"channel_weighting"]
	    caseInsensitiveCompare:@"Linear"] != NSOrderedSame;
	aopt.alpha_weight = opts[@"alpha_weight"] != nil;
	aopt.normal = normal;

	if ([filter caseInsensitiveCompare:@"Box"] == NSOrderedSame)
		which = MIP_FILTER_BOX;
	else if ([filter caseInsensitiveCompare:@"Triangle"] == NSOrderedSame)
		which = MIP_FILTER_TRIANGLE;
	if ([opts[@"wrap_mode"] caseInsensitiveCompare:@"Clamp"] ==
	    NSOrderedSame)
		wrap = MIP_WRAP_CLAMP;
	else if ([opts[@"wrap_mode"] caseInsensitiveCompare:@"Repeat"] ==
	    NSOrderedSame)
		wrap = MIP_WRAP_REPEAT;

	printf("Using Compressor: %s\n", [compressor UTF8String]);

	if ((levels[0] = load_rgba(path, alpha_mode_of(opts), &widths[0],
	    &heights[0])) == NULL) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	flip_image(levels[0], widths[0], heights[0],
	    opts[@"flip_x"] != nil, opts[@"flip_y"] != nil);
	/*
	 * --gamma_in takes the colour to linear before anything samples it,
	 * so the chain is filtered in linear space; --gamma_out puts every
	 * level back afterwards, the premultiply included.
	 *
	 * Neither runs for a normal map, for the reason the premultiply
	 * does not: the channels are a direction rather than a colour, and
	 * a transfer function over them means nothing.  Apple write the
	 * same texels with --normal_map --gamma_in=2.2 as with --normal_map
	 * alone, at every level.
	 */
	if (!normal && [opts[@"gamma_in"] floatValue] != 1.0f)
		image_gamma(levels[0], widths[0], heights[0],
		    [opts[@"gamma_in"] floatValue], 1);
	levels[0] = fit_extent(levels[0], &widths[0], &heights[0],
	    [opts[@"max_extent"] intValue], which, wrap);
	if (normal)
		normalize_normals(levels[0], widths[0], heights[0]);
	n = 1;
	maxlevels = [opts[@"max_mipmaps"] intValue];
	if (maxlevels <= 0 || maxlevels > MAX_LEVELS)
		maxlevels = MAX_LEVELS;
	while (n < maxlevels && (widths[n - 1] > 1 || heights[n - 1] > 1)) {
		levels[n] = mip_downsample(levels[n - 1], widths[n - 1],
		    heights[n - 1], which, wrap, &widths[n], &heights[n]);
		if (levels[n] == NULL)
			break;
		if (normal)
			normalize_normals(levels[n], widths[n], heights[n]);
		n++;
	}

	/*
	 * Not for a normal map: its colour is a direction and folding alpha
	 * into it would mean nothing.  Apple's --normal_map
	 * --alpha_mode=Premultiply writes the same texels as --normal_map
	 * alone, so the premultiply is simply not done.
	 */
	if (!normal && alpha_mode_of(opts) == ALPHA_PREMULTIPLY)
		premultiply_base(levels[0], widths[0], heights[0]);

	/*
	 * A gamma of one is no gamma at all, and skipping it is not just an
	 * optimisation: the exponentiation clamps at zero, and the Kaiser
	 * filter undershoots there, so running it would quietly lift every
	 * negative sample the chain produced.
	 */
	if (!normal && [opts[@"gamma_out"] floatValue] != 1.0f) {
		for (i = 0; i < n; i++)
			image_gamma(levels[i], widths[i], heights[i],
			    [opts[@"gamma_out"] floatValue], 0);
	}

	for (i = 0; i < n; i++) {
		if ([compressor isEqualToString:@"RAW"])
			blocks[i] = pack_raw(levels[i], widths[i], heights[i],
			    [fmt UTF8String], &sizes[i]);
		else if ([compressor isEqualToString:@"NVTT"])
			blocks[i] = compress_bc(levels[i], widths[i],
			    heights[i], bc, aopt.quality, &sizes[i]);
		else if ([compressor isEqualToString:@"STB"])
			blocks[i] = compress_bc_stb(levels[i], widths[i],
			    heights[i], bc, aopt.quality, &sizes[i]);
		else if ([compressor isEqualToString:@"ETC2COMP"])
			blocks[i] = compress_etc(levels[i], widths[i],
			    heights[i], etc, aopt.quality, aopt.perceptual,
			    &sizes[i]);
		else
			blocks[i] = compress_astc(levels[i], widths[i],
			    heights[i], &aopt, &sizes[i]);
		free(levels[i]);
		if (blocks[i] == NULL) {
			printf("Error: Compression failed!\n");
			return (255);
		}
	}

	/*
	 * --srgb_format asks for the sRGB spelling of the format, which is
	 * a different enumerant in both containers and, for ASTC, a Metal
	 * one eighteen below the linear one.  The formats that carry no
	 * colour have no such spelling, and only the two containers have to
	 * name one: the .h and DDS outputs write atcFormatUnknown and
	 * refuse, which they do on their own.  Apple's tool crashes here
	 * rather than checking, so this is an error of our own.
	 */
	if (srgb && (wants_ktx2(output) ||
	    (!wants_header(output) && !wants_dds(output)))) {
		uint32_t vk;

		if (!format_srgb_for([fmt UTF8String], &gl, &vk)) {
			printf("Error: Compression format \"%s\" has no "
			    "sRGB pixel format!\n", [fmt UTF8String]);
			return (255);
		}
		if (metal != 0)
			metal -= 18;
	}

	{
		NSString *options = tc_options_string(opts, compressor, fmt);
		bool annotate = opts[@"disable_annotation"] == nil;

		data = wants_dds(output) ?
		    write_dds_generic(blocks, sizes, widths, heights, n,
		        [fmt UTF8String], srgb) :
		    wants_header(output) ?
		    write_header_generic(blocks, sizes, widths, heights, n,
		        [fmt UTF8String], srgb, output, opts) :
		    wants_ktx2(output) ?
		    write_ktx2_generic(blocks, sizes, widths, heights, n,
		        [fmt UTF8String],
		        alpha_mode_of(opts) == ALPHA_PREMULTIPLY, srgb,
		        options, annotate) :
		    write_ktx_generic(blocks, sizes, widths, heights, n,
		        gl, base, type, type_size, gl_format, texel, metal,
		        alpha_mode_of(opts) == ALPHA_PREMULTIPLY, options,
		        annotate);
		if (data == nil) {
			if (wants_dds(output))
				return (0);
			printf("Error: Could not write output file!\n");
			return (255);
		}
	}
	for (i = 0; i < n; i++)
		free(blocks[i]);

	if (output.length == 0) {
		printf("Error: Missing output file path!\n");
		return (255);
	}
	if (![data writeToFile:output atomically:NO]) {
		printf("Error: Could not write output file!\n");
		return (255);
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* Decompress.                                                         */
/* ------------------------------------------------------------------ */

/*
 * Which decoder reads a format, by the name this tool prints for it.  ASTC
 * goes to astcenc and everything else to NVTT.  etc2comp has no decoder at
 * all, which is why Apple's --decompressor list names it nowhere.
 *
 * Four formats are missing from this table on purpose, because NVTT cannot
 * read them:
 *
 *	BC7		its decoder is the old avpcl prototype, whose
 *			mode 0 header layout is not the one the format
 *			was standardised with.  Handed a conforming
 *			block -- including one its own encoder just
 *			wrote -- it fails an assertion and calls exit.
 *	EAC_R11		nvtt/Surface.cpp has the call sites for these
 *	EAC_RG11	three commented out and marked "@@ Not
 *	ETC2_RGB8A1	implemented".
 *
 * Leaving them out is what stops the assertion from taking the process
 * down with no message at all.
 */
static bool
decode_format_of(const char *name, enum tc_decode *out)
{
	static const struct { const char *name; enum tc_decode dec; } decs[] = {
		{ "BC1", TC_DEC_BC1 }, { "BC2", TC_DEC_BC2 },
		{ "BC3", TC_DEC_BC3 }, { "BC4", TC_DEC_BC4 },
		{ "BC5", TC_DEC_BC5 }, { "BC6U", TC_DEC_BC6 },
		{ "BC6S", TC_DEC_BC6S },
		{ "ETC2_RGB8", TC_DEC_ETC2_RGB },
		{ "EAC_RGBA8", TC_DEC_ETC2_RGBA }
	};
	size_t i;

	for (i = 0; i < sizeof(decs) / sizeof(decs[0]); i++) {
		if (strcmp(decs[i].name, name) == 0) {
			*out = decs[i].dec;
			return (true);
		}
	}
	return (false);
}

/*
 * Pack a decoded RGBA float image down to the channels the output format
 * carries.
 *
 * The rows come out tight.  Version 1 pads them to four bytes -- a level is
 * stored as if read with an UNPACK_ALIGNMENT of 4, so a 2x2 level of R8 is
 * eight bytes and a 1x1 level four -- and version 2 does not, so the writer
 * that wants the padding puts it in.
 */
static uint8_t *
pack_bytes_u8(const uint8_t *rgba, int w, int h, int channels,
    size_t *out_len)
{
	size_t stride = (size_t)w * (size_t)channels;
	uint8_t *out = calloc((size_t)h, stride);
	int x, y, c;

	if (out == NULL)
		return (NULL);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < channels; c++)
				out[(size_t)y * stride +
				    (size_t)x * (size_t)channels + (size_t)c] =
				    rgba[((size_t)y * w + x) * 4 + (size_t)c];
		}
	}
	*out_len = (size_t)h * stride;
	return (out);
}

/*
 * A sample as a byte, through sixteen bits: rounded to a sixteen bit unorm
 * and then reduced to its top byte.  It is the same narrowing EAC uses in
 * the other direction, and nothing in one step matches it -- rounding by
 * 255 is out on one sample in thirty, and truncating by 256 agrees
 * everywhere but the few where the sixteen bit rounding carries, one
 * sample in twenty thousand.
 *
 * Every sample the decompress path produces is already a multiple of
 * 1/255, and k/255 rounds to 257k here, whose top byte is k, so it is only
 * the compression path's mip levels that can tell any of these apart.
 */
static uint8_t
to_byte(float v)
{
	if (!(v > 0.0f))
		return (0);
	if (v >= 1.0f)
		return (255);
	return ((uint8_t)((int)(v * 65535.0f + 0.5f) >> 8));
}

static uint8_t *
pack_bytes(const float *rgba, int w, int h, int channels, size_t *out_len)
{
	size_t stride = (size_t)w * (size_t)channels;
	uint8_t *out = calloc((size_t)h, stride);
	int x, y, c;

	if (out == NULL)
		return (NULL);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			for (c = 0; c < channels; c++)
				out[(size_t)y * stride +
				    (size_t)x * (size_t)channels + (size_t)c] =
				    to_byte(rgba[((size_t)y * w + x) * 4 +
				    (size_t)c]);
		}
	}
	*out_len = (size_t)h * stride;
	return (out);
}

/* BGRA8: the same bytes as RGBA8 with red and blue exchanged. */
static uint8_t *
pack_bytes_bgra(const float *rgba, int w, int h, size_t *out_len)
{
	uint8_t *out = pack_bytes(rgba, w, h, 4, out_len);
	size_t i;

	if (out == NULL)
		return (NULL);
	for (i = 0; i + 3 < *out_len; i += 4) {
		uint8_t t = out[i];

		out[i] = out[i + 2];
		out[i + 2] = t;
	}
	return (out);
}

/*
 * The uncompressed formats, as --compression_format asks for them: the
 * levels the mip chain produced, packed to the named channel count and
 * width.  Apple call this compressor RAW, and it is one -- there is no
 * encoder behind it.
 *
 * Eight bit channels quantise; sixteen and thirty-two bit ones are floats
 * and keep what they were given, so an HDR source is not clipped.  BGRA8 is
 * RGBA8 with the colour channels reversed, which is the only format here
 * whose channels are not in order.  Rows are padded to four bytes as
 * version 1 wants, which is what pack_bytes does for the byte formats.
 */
/*
 * A sample as a half.  Not __fp16, which rounds a tie to even: Apple round
 * a tie up, so 0.4659423828125 -- exactly between two halves -- comes out
 * 0x3775 where the hardware conversion gives 0x3774.  Adding half of the
 * low bit kept before the shift is what does it.
 */
static uint16_t
to_half(float f)
{
	uint32_t u, sign;
	int exp, shift;

	memcpy(&u, &f, sizeof(u));
	sign = (u >> 16) & 0x8000;
	u &= 0x7fffffff;

	if (u >= 0x7f800000)		/* infinity, or not a number */
		return ((uint16_t)(sign | 0x7c00 |
		    ((u & 0x007fffff) ? 0x0200 : 0)));
	if (u >= 0x477ff000)		/* rounds past the largest half */
		return ((uint16_t)(sign | 0x7c00));
	if (u >= 0x38800000)		/* a normal half */
		return ((uint16_t)(sign |
		    (((u + 0x00001000) - 0x38000000) >> 13)));
	if (u < 0x33000000)		/* rounds to zero */
		return ((uint16_t)sign);
	/* Subnormal: put the hidden bit back and round at the same place. */
	exp = (int)(u >> 23);
	shift = 126 - exp;
	u = (u & 0x007fffff) | 0x00800000;
	return ((uint16_t)(sign | ((u + (1u << (shift - 1))) >> shift)));
}

static void *
pack_raw(const float *rgba, int w, int h, const char *name, size_t *out_len)
{
	uint32_t gl, base, metal;
	int bx, by, channels, bits, x, y, c;
	uint8_t *out;
	size_t stride;

	if (!format_lookup(name, &gl, &base, &bx, &by, &metal) || bx != 1)
		return (NULL);
	channels = base == 0x1903 ? 1 : base == 0x8227 ? 2 :
	    base == 0x1907 ? 3 : 4;
	bits = format_channel_bits(name);
	if (bits == 8)
		return (name[0] == 'B' ?
		    pack_bytes_bgra(rgba, w, h, out_len) :
		    pack_bytes(rgba, w, h, channels, out_len));

	stride = (size_t)w * (size_t)channels * (size_t)(bits / 8);
	if ((out = calloc((size_t)h, stride)) == NULL)
		return (NULL);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			const float *px = rgba + ((size_t)y * w + x) * 4;
			uint8_t *o = out + (size_t)y * stride +
			    (size_t)x * channels * (bits / 8);

			for (c = 0; c < channels; c++) {
				if (bits == 32) {
					memcpy(o + c * 4, &px[c], 4);
				} else {
					uint16_t h16 = to_half(px[c]);

					memcpy(o + c * 2, &h16, 2);
				}
			}
		}
	}
	*out_len = (size_t)h * stride;
	return (out);
}

/*
 * Read a compressed container and write an uncompressed one beside it.  The
 * output format is not a choice: it follows the channels the source format
 * carries, which is what its base internal format records -- one channel
 * becomes R8, two RG8, three RGB8 and four RGBA8.  BC6 is the exception, an
 * HDR format whose samples do not fit in a byte, and Apple write RGBA32 for
 * it.
 */
static int
do_decompress(NSString *path, NSDictionary<NSString *, NSString *> *opts)
{
	enum { MAX_LEVELS = 32 };
	void *outs[MAX_LEVELS];
	size_t sizes[MAX_LEVELS];
	int widths[MAX_LEVELS], heights[MAX_LEVELS];
	NSString *out = opts[@"decompressed"];
	NSData *data = [NSData dataWithContentsOfFile:path];
	struct ktx k;
	const char *name;
	enum tc_decode dec = TC_DEC_BC1;
	uint32_t gl, base, metal, ogl, obase, ometal;
	int bx, by, obx, oby, i, n;
	bool astc, eac, hdr;
	const char *oname;

	if (out.length == 0) {
		printf("Error: No decompressed output path specified!\n");
		short_usage();
		return (255);
	}
	if (data == nil || !ktx_parse([data bytes], [data length], &k)) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	name = k.version == 1 ? format_name_for_gl(k.gl_internal_format) :
	    format_name_for_vk(k.vk_format);
	if (name == NULL || !format_lookup(name, &gl, &base, &bx, &by,
	    &metal) || bx == 1) {
		printf("Error: Unsupported compression format!\n");
		ktx_free(&k);
		return (255);
	}
	astc = strncmp(name, "ASTC", 4) == 0;
	eac = strcmp(name, "EAC_R11") == 0 || strcmp(name, "EAC_RG11") == 0;
	hdr = strcmp(name, "BC6U") == 0 || strcmp(name, "BC6S") == 0;
	if (!astc && !eac && !decode_format_of(name, &dec)) {
		printf("Error: Decompressing \"%s\" is not implemented in "
		    "this build!\n", name);
		ktx_free(&k);
		return (255);
	}

	if (hdr)
		oname = "RGBA32";
	else if (base == 0x1903)
		oname = "R8";
	else if (base == 0x8227)
		oname = "RG8";
	else if (base == 0x1907)
		oname = "RGB8";
	else
		oname = "RGBA8";
	if (!format_lookup(oname, &ogl, &obase, &obx, &oby, &ometal)) {
		ktx_free(&k);
		return (255);
	}

	n = (int)k.nlevel;
	if (n > MAX_LEVELS)
		n = MAX_LEVELS;
	for (i = 0; i < n; i++) {
		int channels = strcmp(oname, "R8") == 0 ? 1 :
		    strcmp(oname, "RG8") == 0 ? 2 :
		    strcmp(oname, "RGB8") == 0 ? 3 : 4;

		widths[i] = (int)k.level[i].width;
		heights[i] = (int)k.level[i].height;
		/*
		 * ASTC goes straight to eight bits.  Decoding to float and
		 * rounding after is not the same answer -- it is off by one
		 * either way in a few dozen samples per level -- and Apple's
		 * is astcenc's own quantisation.
		 */
		if (astc) {
			uint8_t *px = decode_astc_u8(k.level[i].data,
			    k.level[i].len, widths[i], heights[i], bx, by);

			if (px == NULL) {
				printf("Error: Decompression failed!\n");
				ktx_free(&k);
				return (255);
			}
			outs[i] = pack_bytes_u8(px, widths[i], heights[i],
			    channels, &sizes[i]);
			free(px);
		} else {
			float *pixels = eac ?
			    decode_eac(k.level[i].data, k.level[i].len,
			        widths[i], heights[i],
			        strcmp(name, "EAC_RG11") == 0) :
			    decode_blocks(k.level[i].data, k.level[i].len,
			        widths[i], heights[i], dec);

			if (pixels == NULL) {
				printf("Error: Decompression failed!\n");
				ktx_free(&k);
				return (255);
			}
			if (hdr) {
				outs[i] = pixels;
				sizes[i] = (size_t)widths[i] * heights[i] *
				    4 * sizeof(float);
				continue;
			}
			outs[i] = pack_bytes(pixels, widths[i], heights[i],
			    channels, &sizes[i]);
			free(pixels);
		}
		if (outs[i] == NULL) {
			ktx_free(&k);
			return (255);
		}
	}
	ktx_free(&k);

	{
		NSString *options = tc_options_string(opts, nil, nil);
		bool annotate = opts[@"disable_annotation"] == nil;
		NSData *file;

		if (wants_dds(out)) {
			file = write_dds_generic(outs, sizes, widths,
			    heights, n, oname, false);
		} else if (wants_header(out)) {
			file = write_header_generic(outs, sizes, widths,
			    heights, n, oname, false, out, opts);
		} else if (wants_ktx2(out)) {
			file = write_ktx2_generic(outs, sizes, widths,
			    heights, n, oname, false, false, options,
			    annotate);
		} else {
			uint32_t texel = hdr ? 16 : (uint32_t)(
			    obase == 0x1903 ? 1 : obase == 0x8227 ? 2 :
			    obase == 0x1907 ? 3 : 4);

			file = write_ktx_generic(outs, sizes, widths, heights,
			    n, ogl, obase, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE,
			    hdr ? 4 : 1, obase, texel, 0, false, options,
			    annotate);
		}

		for (i = 0; i < n; i++)
			free(outs[i]);
		if (file == nil) {
			if (wants_dds(out))
				return (0);
			printf("Error: Could not write output file!\n");
			return (255);
		}
		if (![file writeToFile:out atomically:NO]) {
			printf("Error: Could not write output file!\n");
			return (255);
		}
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* Compare.                                                            */
/* ------------------------------------------------------------------ */

enum { CMP_MAX_LEVELS = 32 };

/*
 * Levels held as RGBA float.  Not bytes: BC6 decodes above 1.0 and Apple do
 * not clamp it before measuring, so clamping here would report a smaller
 * error than theirs.  Everything that arrives as eight bits is divided by
 * 255 and multiplied back by 255 for the measure, which is exact.
 */
struct cmp_image {
	float	*level[CMP_MAX_LEVELS];		/* w*h*4, no row padding */
	int	 w[CMP_MAX_LEVELS], h[CMP_MAX_LEVELS];
	int	 n;
};

static void
cmp_free(struct cmp_image *im)
{
	int i;

	for (i = 0; i < im->n; i++)
		free(im->level[i]);
	im->n = 0;
}

/* Bytes to the floats that name them exactly.  Frees what it is given. */
static float *
bytes_to_float(uint8_t *px, size_t n)
{
	float *out = malloc(n * sizeof(*out));
	size_t i;

	if (out != NULL) {
		for (i = 0; i < n; i++)
			out[i] = px[i] * (1.0f / 255.0f);
	}
	free(px);
	return (out);
}

/*
 * One level of an uncompressed container, whatever it stores, as RGBA.
 * Rows are padded to four bytes on the way in and not on the way out.
 */
static float *
unpack_level(const struct ktx_level *lv, bool is_float, int channels)
{
	float *out = calloc((size_t)lv->width * lv->height * 4, sizeof(*out));
	size_t stride;
	uint32_t x, y;
	int c;

	if (out == NULL)
		return (NULL);
	stride = is_float ?
	    (size_t)lv->width * (size_t)channels * sizeof(float) :
	    (((size_t)lv->width * (size_t)channels + 3) & ~(size_t)3);
	if (stride * lv->height > lv->len) {
		free(out);
		return (NULL);
	}
	for (y = 0; y < lv->height; y++) {
		for (x = 0; x < lv->width; x++) {
			float *o = out + ((size_t)y * lv->width + x) * 4;

			o[3] = 1.0f;
			for (c = 0; c < channels; c++) {
				if (is_float)
					memcpy(&o[c], lv->data + y * stride +
					    ((size_t)x * channels + c) *
					    sizeof(float), sizeof(o[c]));
				else
					o[c] = lv->data[y * stride +
					    (size_t)x * channels + c] *
					    (1.0f / 255.0f);
			}
		}
	}
	return (out);
}

/*
 * Load anything this tool can read as a stack of RGBA levels: a container,
 * compressed or not, or an image file, which has one level.
 */
static bool
cmp_load(NSString *path, struct cmp_image *im)
{
	NSData *data = [NSData dataWithContentsOfFile:path];
	struct ktx k;
	const char *name;
	uint32_t gl, base, metal;
	int bx, by, i;
	bool hdr;

	memset(im, 0, sizeof(*im));
	if (data == nil)
		return (false);

	if (!ktx_parse([data bytes], [data length], &k)) {
		float *px;
		int w, h;

		/* Not a container: an image file, and one level of it. */
		if ((px = load_rgba(path, ALPHA_PRESERVE, &w, &h)) == NULL)
			return (false);
		im->level[0] = px;
		im->w[0] = w;
		im->h[0] = h;
		im->n = 1;
		return (true);
	}

	name = k.version == 1 ? format_name_for_gl(k.gl_internal_format) :
	    format_name_for_vk(k.vk_format);
	if (name == NULL || !format_lookup(name, &gl, &base, &bx, &by,
	    &metal)) {
		ktx_free(&k);
		return (false);
	}
	hdr = strcmp(name, "BC6U") == 0 || strcmp(name, "BC6S") == 0;
	im->n = (int)k.nlevel;
	if (im->n > CMP_MAX_LEVELS)
		im->n = CMP_MAX_LEVELS;
	for (i = 0; i < im->n; i++) {
		const struct ktx_level *lv = &k.level[i];

		im->w[i] = (int)lv->width;
		im->h[i] = (int)lv->height;
		if (bx == 1) {
			int channels = base == 0x1903 ? 1 :
			    base == 0x8227 ? 2 : base == 0x1907 ? 3 : 4;

			im->level[i] = unpack_level(lv, format_is_float(name),
			    channels);
		} else if (strncmp(name, "ASTC", 4) == 0) {
			/*
			 * Through the eight bit decode, not the float one.
			 * The two are not the same answer and Apple's is
			 * this one, the same quantisation their
			 * --mode=decompress writes.
			 */
			uint8_t *px = decode_astc_u8(lv->data, lv->len,
			    im->w[i], im->h[i], bx, by);

			im->level[i] = px == NULL ? NULL :
			    bytes_to_float(px,
			        (size_t)im->w[i] * im->h[i] * 4);
		} else if (strcmp(name, "EAC_R11") == 0 ||
		    strcmp(name, "EAC_RG11") == 0) {
			im->level[i] = decode_eac(lv->data, lv->len,
			    im->w[i], im->h[i],
			    strcmp(name, "EAC_RG11") == 0);
		} else {
			enum tc_decode dec;
			float *px = decode_format_of(name, &dec) ?
			    decode_blocks(lv->data, lv->len, im->w[i],
			        im->h[i], dec) : NULL;

			/*
			 * Everything but BC6 is an eight bit image, and is
			 * quantised to eight bits here for the same reason
			 * --mode=decompress writes bytes for it: comparing
			 * NVTT's float against the same value that has been
			 * through a byte is otherwise off by a fraction of a
			 * unit in the last place, which is enough to report
			 * a difference where Apple report none.
			 */
			if (px != NULL && !hdr) {
				size_t j, n = (size_t)im->w[i] * im->h[i] * 4;

				for (j = 0; j < n; j++) {
					float v = px[j];

					if (v < 0.0f)
						v = 0.0f;
					if (v > 1.0f)
						v = 1.0f;
					px[j] = (float)lrintf(255.0f * v) *
					    (1.0f / 255.0f);
				}
			}
			im->level[i] = px;
		}
		if (im->level[i] == NULL) {
			im->n = i;
			cmp_free(im);
			ktx_free(&k);
			return (false);
		}
	}
	ktx_free(&k);
	return (true);
}

/*
 * Report how far apart two images are.
 *
 * The measure is Apple's, read off their tool: the mean squared error of
 * red, green and blue in eight bit units, pooled over every mip level
 * rather than taken per level and averaged, with alpha ignored entirely --
 * two images whose colour matches and whose alpha does not are reported
 * identical.  RMS is its square root and PSNR is 10*log10(255^2/MSE).
 *
 * A mismatch in size or level count is named and nothing is measured; their
 * tool still prints the zeroed line after it, so this does too.
 *
 * Where their own compare is self-consistent this agrees with it exactly.
 * It often is not, and the shape of that is worth writing down, because it
 * is the whole of the remaining difference:
 *
 * Their compare does not read a compressed container through their own
 * decompressor.  Decompress a 64x64 BC1 file with their tool, compare the
 * result against an ASTC file, and they answer 603.47; compare the BC1 file
 * itself against the same ASTC decompressed the same way and they answer
 * 639.19.  One of those two numbers is measured against a decode they
 * published and the other is not.  Asked to compare a compressed file with
 * the very file its own decompress produced from it, they report a
 * difference for BC4, ETC2_RGB8, EAC_R11 and EAC_RG11 -- and for BC5 a
 * difference of exactly zero followed by PSNR:1.79...e308, which is
 * 10*log10(255^2/0), so their identity test and their measure disagree and
 * the divide by zero is unguarded.
 *
 * This reads every container through the decoders the rest of the tool
 * uses, so its answers agree with its own --mode=decompress throughout.
 */
static int
do_compare(NSString *path, NSDictionary<NSString *, NSString *> *opts)
{
	NSString *other = opts[@"compare"];
	struct cmp_image a, b;
	double se = 0.0, mse;
	size_t count = 0;
	int i;

	if (other.length == 0) {
		printf("Error: No comparison path specified!\n");
		short_usage();
		return (255);
	}
	/*
	 * A format with no decoder here cannot be measured, and cmp_load
	 * says so by failing.  Those are the same four --mode=decompress
	 * refuses, for the same reason.
	 */
	if (!cmp_load(path, &a)) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	if (!cmp_load(other, &b)) {
		cmp_free(&a);
		printf("Error: Could not read comparison file!\n");
		return (255);
	}

	if (a.n != b.n)
		printf("NumMipmaps Differ: (%d != %d)\n", a.n, b.n);
	else if (a.w[0] != b.w[0])
		printf("Widths Differ: (%d != %d)\n", a.w[0], b.w[0]);
	else if (a.h[0] != b.h[0])
		printf("Heights Differ: (%d != %d)\n", a.h[0], b.h[0]);
	else {
		for (i = 0; i < a.n; i++) {
			size_t j, n = (size_t)a.w[i] * a.h[i];

			for (j = 0; j < n; j++) {
				int c;

				for (c = 0; c < 3; c++) {
					double d = 255.0 *
					    ((double)a.level[i][j * 4 + c] -
					    (double)b.level[i][j * 4 + c]);

					se += d * d;
					count++;
				}
			}
		}
	}
	cmp_free(&a);
	cmp_free(&b);

	if (count == 0) {
		printf("Images differ. RMS:0.00 MSE:0.00 PSNR:0.00\n");
		return (0);
	}
	mse = se / (double)count;
	if (mse == 0.0) {
		printf("Images are identical\n");
		return (0);
	}
	printf("Images differ. RMS:%.2f MSE:%.2f PSNR:%.2f\n", sqrt(mse), mse,
	    10.0 * log10(255.0 * 255.0 / mse));
	return (0);
}

/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
@autoreleasepool {
	NSMutableDictionary<NSString *, NSString *> *opts =
	    [NSMutableDictionary dictionary];
	NSMutableArray<NSString *> *inputs = [NSMutableArray array];
	const struct option_def *o;
	NSString *mode;
	int i;

	progpath = argv[0];
	for (o = options; o->name != NULL; o++) {
		if (o->deflt != NULL)
			opts[[NSString stringWithUTF8String:o->name]] =
			    [NSString stringWithUTF8String:o->deflt];
	}

	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		compat_usage();
		return (255);
	}

	for (i = 1; i < argc; i++) {
		const char *a = argv[i], *eq;
		size_t namelen;

		if (a[0] != '-' || a[1] != '-') {
			[inputs addObject:[NSString stringWithUTF8String:a]];
			continue;
		}
		a += 2;
		eq = strchr(a, '=');
		namelen = eq != NULL ? (size_t)(eq - a) : strlen(a);
		if ((o = find_option(a, namelen)) == NULL) {
			printf("Error: Did not recognize parameter %d '%s'\n",
			    i, argv[i]);
			full_usage();
			return (255);
		}
		opts[[NSString stringWithUTF8String:o->name]] = eq != NULL ?
		    [NSString stringWithUTF8String:eq + 1] : @"1";
	}

	if (inputs.count == 0) {
		printf("Error: Missing input file path!\n");
		short_usage();
		return (255);
	}

	mode = opts[@"mode"];
	if ([mode caseInsensitiveCompare:@"examine"] == NSOrderedSame)
		return (do_examine(inputs[0]));
	if ([mode caseInsensitiveCompare:@"convert"] == NSOrderedSame)
		return (do_convert(inputs[0], opts));
	if ([mode caseInsensitiveCompare:@"compress"] == NSOrderedSame)
		return (do_compress(inputs[0], opts));

	if ([mode caseInsensitiveCompare:@"decompress"] == NSOrderedSame)
		return (do_decompress(inputs[0], opts));

	if ([mode caseInsensitiveCompare:@"compare"] == NSOrderedSame)
		return (do_compare(inputs[0], opts));

	printf("Error: Mode \"%s\" is not implemented in this build!\n",
	    [mode UTF8String]);
	return (255);
}
}
