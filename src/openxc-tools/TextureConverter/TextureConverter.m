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
 * One difference is inherent and cannot be helped: Apple stamp their build
 * number into every file they write and into their usage banner, and ours
 * is not their build.  --disable_annotation turns the file stamping off.
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

	/*
	 * The remaining modes -- Compress, Convert, Decompress, Compare --
	 * are not written yet.  Saying so beats pretending: the compressors
	 * they would drive are ports in this tree already (ASTC, BC, ETC2,
	 * BC6H/BC7), but the containers and the pixel pipeline around them
	 * are still to come.
	 */
	printf("Error: Mode \"%s\" is not implemented in this build!\n",
	    [mode UTF8String]);
	return (255);
}
}
