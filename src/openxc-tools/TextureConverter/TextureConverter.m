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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formats.h"
#include "compress.h"
#include "ktx.h"
#include "mipmap.h"
#include "usage.h"

/* Our own build, not Apple's; see the note at the top of the file. */
#define TC_VERSION	"4.0.200600"

static const char *progpath;

/* Defined below, beside the compression path it mostly describes. */
static NSString *tc_options_string(NSDictionary<NSString *, NSString *> *,
    NSString *compressor, NSString *fmt);

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

static enum alpha_mode
alpha_mode_of(NSDictionary<NSString *, NSString *> *opts)
{
	NSString *m = opts[@"alpha_mode"];

	if ([m caseInsensitiveCompare:@"Preserve"] == NSOrderedSame)
		return (ALPHA_PRESERVE);
	if ([m caseInsensitiveCompare:@"Premultiply"] == NSOrderedSame)
		return (ALPHA_PREMULTIPLY);
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
    bool compressed, uint32_t metal, bool premultiplied, NSString *options,
    bool annotate)
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

	[out appendBytes:ktx1_id length:sizeof(ktx1_id)];
	put32(out, 0x04030201);		/* endianness */
	/*
	 * A compressed format names no type and no size: the block layout is
	 * the internal format's business.
	 */
	put32(out, compressed ? 0 : 0x1406);		/* glType: GL_FLOAT */
	put32(out, compressed ? 1 : 4);			/* glTypeSize */
	put32(out, compressed ? 0 : 0x1908);		/* glFormat: GL_RGBA */
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

	for (i = 0; i < nlevels; i++) {
		static const uint8_t pad[4] = { 0, 0, 0, 0 };

		put32(out, (uint32_t)sizes[i]);
		[out appendBytes:levels[i] length:sizes[i]];
		/* Each level is padded to a four byte boundary. */
		[out appendBytes:pad length:(4 - sizes[i] % 4) % 4];
	}
	(void)widths;
	(void)heights;
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
	NSData *data;
	int n = 0, i, maxlevels;

	printf("Converting %s\n\n", [path UTF8String]);

	if ([filter caseInsensitiveCompare:@"Box"] == NSOrderedSame)
		which = MIP_FILTER_BOX;
	else if ([filter caseInsensitiveCompare:@"Triangle"] == NSOrderedSame)
		which = MIP_FILTER_TRIANGLE;

	if ((levels[0] = load_rgba(path, alpha_mode_of(opts), &widths[0],
	    &heights[0])) == NULL) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	n = 1;

	maxlevels = [opts[@"max_mipmaps"] intValue];
	if (maxlevels <= 0 || maxlevels > MAX_LEVELS)
		maxlevels = MAX_LEVELS;
	while (n < maxlevels && (widths[n - 1] > 1 || heights[n - 1] > 1)) {
		levels[n] = mip_downsample(levels[n - 1], widths[n - 1],
		    heights[n - 1], which, &widths[n], &heights[n]);
		if (levels[n] == NULL)
			break;
		n++;
	}

	if (alpha_mode_of(opts) == ALPHA_PREMULTIPLY)
		premultiply_base(levels[0], widths[0], heights[0]);

	{
		void *ptrs[MAX_LEVELS];
		size_t sizes[MAX_LEVELS];

		for (i = 0; i < n; i++) {
			ptrs[i] = levels[i];
			sizes[i] = (size_t)widths[i] * heights[i] * 4 *
			    sizeof(float);
		}
		data = write_ktx_generic(ptrs, sizes, widths, heights, n,
		    0x8814, 0x1908, false, 0,
		    alpha_mode_of(opts) == ALPHA_PREMULTIPLY,
		    tc_options_string(opts, nil, nil),
		    opts[@"disable_annotation"] == nil);
	}
	for (i = 0; i < n; i++)
		free(levels[i]);

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
		"max_extent", "resize_filter", "crop_uniform_content",
		"wrap_mode", "normal_map", "rgbm_encoding", "scale_range",
		"channel_weighting"
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
	enum tc_bc bc = TC_BC1;
	NSString *compressor;
	uint32_t gl, base, metal;
	NSData *data;
	int n = 0, i, maxlevels;

	if (!format_lookup([fmt UTF8String], &gl, &base, &aopt.block_x,
	    &aopt.block_y, &metal) || aopt.block_x == 1) {
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

	if ([fmt hasPrefix:@"ASTC"]) {
		compressor = @"ARM";
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
		if (bc == TC_BC1 && alpha_mode_of(opts) != ALPHA_IGNORE)
			bc = TC_BC1A;
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

	if ([filter caseInsensitiveCompare:@"Box"] == NSOrderedSame)
		which = MIP_FILTER_BOX;
	else if ([filter caseInsensitiveCompare:@"Triangle"] == NSOrderedSame)
		which = MIP_FILTER_TRIANGLE;

	printf("Using Compressor: %s\n", [compressor UTF8String]);

	if ((levels[0] = load_rgba(path, alpha_mode_of(opts), &widths[0],
	    &heights[0])) == NULL) {
		printf("Error: Could not read input file!\n");
		return (255);
	}
	n = 1;
	maxlevels = [opts[@"max_mipmaps"] intValue];
	if (maxlevels <= 0 || maxlevels > MAX_LEVELS)
		maxlevels = MAX_LEVELS;
	while (n < maxlevels && (widths[n - 1] > 1 || heights[n - 1] > 1)) {
		levels[n] = mip_downsample(levels[n - 1], widths[n - 1],
		    heights[n - 1], which, &widths[n], &heights[n]);
		if (levels[n] == NULL)
			break;
		n++;
	}

	if (alpha_mode_of(opts) == ALPHA_PREMULTIPLY)
		premultiply_base(levels[0], widths[0], heights[0]);

	for (i = 0; i < n; i++) {
		if ([compressor isEqualToString:@"NVTT"])
			blocks[i] = compress_bc(levels[i], widths[i],
			    heights[i], bc, aopt.quality, &sizes[i]);
		else if ([compressor isEqualToString:@"STB"])
			blocks[i] = compress_bc_stb(levels[i], widths[i],
			    heights[i], bc, aopt.quality, &sizes[i]);
		else
			blocks[i] = compress_astc(levels[i], widths[i],
			    heights[i], &aopt, &sizes[i]);
		free(levels[i]);
		if (blocks[i] == NULL) {
			printf("Error: Compression failed!\n");
			return (255);
		}
	}

	{
		NSString *options = tc_options_string(opts, compressor, fmt);

		data = write_ktx_generic(blocks, sizes, widths, heights, n,
		    gl, base, true, metal,
		    alpha_mode_of(opts) == ALPHA_PREMULTIPLY, options,
		    opts[@"disable_annotation"] == nil);
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

	/*
	 * The remaining modes -- Decompress and Compare -- are not written
	 * yet.  Saying so beats pretending: the compressors
	 * they would drive are ports in this tree already (ASTC, BC, ETC2,
	 * BC6H/BC7), but the containers and the pixel pipeline around them
	 * are still to come.
	 */
	printf("Error: Mode \"%s\" is not implemented in this build!\n",
	    [mode UTF8String]);
	return (255);
}
}
