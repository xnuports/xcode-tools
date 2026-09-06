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
#include "ktx.h"
#include "mipmap.h"
#include "usage.h"

/* Our own build, not Apple's; see the note at the top of the file. */
#define TC_VERSION	"4.0.200600"

static const char *progpath;

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

/*
 * Read an image into tightly packed RGBA floats, top row first.  The values
 * are the stored bytes over 255 with no gamma applied, which is what Apple's
 * writes: a level of theirs holds 0.501961 where the source byte was 128.
 */
static float *
load_rgba(NSString *path, int *wp, int *hp)
{
	CGImageSourceRef src;
	CGImageRef img;
	CGColorSpaceRef cs;
	CGContextRef ctx;
	uint8_t *bytes;
	float *out;
	size_t w, h, i, n;

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
	if (w == 0 || h == 0) {
		CGImageRelease(img);
		return (NULL);
	}
	if ((bytes = calloc(1, w * h * 4)) == NULL) {
		CGImageRelease(img);
		return (NULL);
	}
	cs = CGColorSpaceCreateDeviceRGB();
	ctx = CGBitmapContextCreate(bytes, w, h, 8, w * 4, cs,
	    kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
	CGColorSpaceRelease(cs);
	if (ctx == NULL) {
		free(bytes);
		CGImageRelease(img);
		return (NULL);
	}
	CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
	CGContextRelease(ctx);
	CGImageRelease(img);

	n = w * h * 4;
	if ((out = malloc(n * sizeof(*out))) == NULL) {
		free(bytes);
		return (NULL);
	}
	/*
	 * Multiplied by the reciprocal rather than divided by 255, because
	 * that is what Apple's does and the two disagree.  1/255 is not
	 * exact in binary, so the multiply carries its rounding into the
	 * result: 96 comes out 0x3ec0c0c2 that way and 0x3ec0c0c1 by
	 * division -- one unit in the last place, in every file.
	 */
	{
		const float inv255 = 1.0f / 255.0f;

		for (i = 0; i < n; i += 4) {
			unsigned a = bytes[i + 3];

			/* CoreGraphics hands back premultiplied colour. */
			if (a != 0 && a != 255) {
				out[i + 0] = (bytes[i + 0] * 255u / a) * inv255;
				out[i + 1] = (bytes[i + 1] * 255u / a) * inv255;
				out[i + 2] = (bytes[i + 2] * 255u / a) * inv255;
			} else {
				out[i + 0] = bytes[i + 0] * inv255;
				out[i + 1] = bytes[i + 1] * inv255;
				out[i + 2] = bytes[i + 2] * inv255;
			}
			out[i + 3] = a * inv255;
		}
	}
	free(bytes);
	*wp = (int)w;
	*hp = (int)h;
	return (out);
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

/* One key/value pair, NUL terminated and padded to a four byte boundary. */
static void
put_kv(NSMutableData *d, const char *key, const char *value)
{
	size_t klen = strlen(key) + 1, vlen = strlen(value) + 1;
	static const uint8_t pad[4] = { 0, 0, 0, 0 };

	put32(d, (uint32_t)(klen + vlen));
	[d appendBytes:key length:klen];
	[d appendBytes:value length:vlen];
	[d appendBytes:pad length:(4 - (klen + vlen) % 4) % 4];
}

static const uint8_t ktx1_id[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

/*
 * KTX version 1 holding RGBA32F, which is what the conversion path writes
 * whatever --format asks for: Apple's does the same.
 */
static NSData *
write_ktx(float **levels, const int *widths, const int *heights, int nlevels,
    bool annotate)
{
	NSMutableData *out = [NSMutableData data];
	NSMutableData *kvd = [NSMutableData data];
	int i;

	if (annotate) {
		put_kv(kvd, "TC_Version", TC_VERSION);
		put_kv(kvd, "KTXwriter", "TextureConverter " TC_VERSION);
		put_kv(kvd, "TC_Options", "");
	}

	[out appendBytes:ktx1_id length:sizeof(ktx1_id)];
	put32(out, 0x04030201);		/* endianness */
	put32(out, 0x1406);		/* glType: GL_FLOAT */
	put32(out, 4);			/* glTypeSize */
	put32(out, 0x1908);		/* glFormat: GL_RGBA */
	put32(out, 0x8814);		/* glInternalFormat: GL_RGBA32F */
	put32(out, 0x1908);		/* glBaseInternalFormat: GL_RGBA */
	put32(out, (uint32_t)widths[0]);
	put32(out, (uint32_t)heights[0]);
	put32(out, 0);			/* pixelDepth */
	put32(out, 0);			/* numberOfArrayElements */
	put32(out, 1);			/* numberOfFaces */
	put32(out, (uint32_t)nlevels);
	put32(out, (uint32_t)[kvd length]);
	[out appendData:kvd];

	for (i = 0; i < nlevels; i++) {
		uint32_t n = (uint32_t)((size_t)widths[i] * heights[i] * 4 *
		    sizeof(float));

		put32(out, n);
		[out appendBytes:levels[i] length:n];
		/* Levels are padded to four bytes; a float image never is. */
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
	NSData *data;
	int n = 0, i, maxlevels;

	printf("Converting %s\n\n", [path UTF8String]);

	if ([filter caseInsensitiveCompare:@"Box"] == NSOrderedSame)
		which = MIP_FILTER_BOX;
	else if ([filter caseInsensitiveCompare:@"Triangle"] == NSOrderedSame)
		which = MIP_FILTER_TRIANGLE;

	if ((levels[0] = load_rgba(path, &widths[0], &heights[0])) == NULL) {
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

	data = write_ktx(levels, widths, heights, n,
	    opts[@"disable_annotation"] == nil);
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

	/*
	 * The remaining modes -- Compress, Decompress, Compare -- are not
	 * written yet.  Saying so beats pretending: the compressors
	 * they would drive are ports in this tree already (ASTC, BC, ETC2,
	 * BC6H/BC7), but the containers and the pixel pipeline around them
	 * are still to come.
	 */
	printf("Error: Mode \"%s\" is not implemented in this build!\n",
	    [mode UTF8String]);
	return (255);
}
}
