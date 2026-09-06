/*
 * TextureAtlas -- build a SpriteKit texture atlas from a folder of images.
 *
 * The output is a <name>.atlasc bundle: one or more packed textures plus a
 * property list saying where each source image landed.  Images that share a
 * device suffix (@2x, ~ipad and friends) are packed together and written to
 * their own page, since a run-time only ever loads the one matching the
 * device it is on.
 *
 * Each sprite is trimmed of its fully transparent border, then packed with
 * MaxRects and surrounded by a one pixel skirt of duplicated edge pixels so
 * that bilinear filtering cannot bleed a neighbour in.  The atlas is
 * measured in bottom-left coordinates, which is what SpriteKit wants and
 * the reverse of the order the rows are stored in.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#import <Foundation/Foundation.h>

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <err.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "introsort.h"
#include "maxrects.h"

/* -f: the pixel format the pages are written in. */
enum {
	FMT_RGBA8888 = 1,	/* a PNG, and the default */
	FMT_PVR_RGBA8888,	/* the rest are gzipped PVR v2 */
	FMT_PVR_RGBA4444,
	FMT_PVR_RGBA5551,
	FMT_PVR_RGB565,
	FMT_MAX = FMT_PVR_RGB565
};

/*
 * The device suffixes, longest first so that "@2x~iphone" is recognized
 * before "@2x" would swallow it.  A file matching none of them belongs to
 * the default group, whose pages carry no suffix at all.
 */
static NSString * const kSuffixes[] = {
	@"@1080", @"@3x", @"@3x~iphone", @"-568h@2x", @"-568h@2x~iphone",
	@"@2x", @"@2x~iphone", @"@2x~ipad", @"~iphone", @"~ipad"
};
static const size_t kSuffixCount = sizeof(kSuffixes) / sizeof(kSuffixes[0]);

static const char	*progname;
static bool		 opt_copy;	/* -c */
static bool		 opt_pot;	/* -p */
static bool		 opt_quiet;	/* -g */
static bool		 opt_verbose;	/* -v */
static int		 opt_format = FMT_RGBA8888;
static int		 opt_maxdim = 2048;

static void
atlas_error(NSString *fmt, ...)
{
	va_list ap;
	NSString *s;

	va_start(ap, fmt);
	s = [[NSString alloc] initWithFormat:fmt arguments:ap];
	va_end(ap);
	fprintf(stderr, "TextureAtlas: error: %s\n", [s UTF8String]);
}

static void
note(NSString *fmt, ...)
{
	va_list ap;
	NSString *s;

	if (!opt_verbose)
		return;
	va_start(ap, fmt);
	s = [[NSString alloc] initWithFormat:fmt arguments:ap];
	va_end(ap);
	printf("%s\n", [s UTF8String]);
}

/* ------------------------------------------------------------------ */
/* One source image, decoded and trimmed.                             */
/* ------------------------------------------------------------------ */

@interface Sprite : NSObject
@property (nonatomic, strong) NSString	*name;		/* file name */
@property (nonatomic, assign) uint8_t	*rgba;		/* trimmed, RGBA8 */
@property (nonatomic, assign) int	 width;		/* trimmed extent */
@property (nonatomic, assign) int	 height;
@property (nonatomic, assign) int	 sourceWidth;	/* before trimming */
@property (nonatomic, assign) int	 sourceHeight;
@property (nonatomic, assign) int	 offsetX;	/* trimmed from left */
@property (nonatomic, assign) int	 offsetY;	/* trimmed from bottom */
@property (nonatomic, assign) BOOL	 opaque;
/* Filled in by the packer. */
@property (nonatomic, assign) int	 page;
@property (nonatomic, assign) struct mr_rect placed;
@property (nonatomic, assign) BOOL	 rotated;
@end

@implementation Sprite
- (void)dealloc
{
	free(_rgba);
}
@end

/*
 * Decode one file into a tightly packed non-premultiplied RGBA8 buffer.
 * CoreGraphics will not hand back non-premultiplied 8-bit data directly, so
 * the bitmap is drawn premultiplied and then divided back out; a texture
 * atlas has to store what the source file stored, because SpriteKit
 * premultiplies again at load time.
 */
static uint8_t *
decode_image(NSURL *url, int *wp, int *hp)
{
	CGImageSourceRef src;
	CGImageRef img;
	CGColorSpaceRef cs;
	CGContextRef ctx;
	uint8_t *buf;
	size_t w, h, n, i;

	src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
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
	n = w * h * 4;
	if ((buf = calloc(1, n)) == NULL)
		err(1, NULL);

	cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	ctx = CGBitmapContextCreate(buf, w, h, 8, w * 4, cs,
	    kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
	CGColorSpaceRelease(cs);
	if (ctx == NULL) {
		free(buf);
		CGImageRelease(img);
		return (NULL);
	}
	CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
	CGContextRelease(ctx);
	CGImageRelease(img);

	for (i = 0; i < n; i += 4) {
		unsigned a = buf[i + 3];

		if (a == 0 || a == 255)
			continue;
		buf[i + 0] = (uint8_t)((buf[i + 0] * 255 + a / 2) / a);
		buf[i + 1] = (uint8_t)((buf[i + 1] * 255 + a / 2) / a);
		buf[i + 2] = (uint8_t)((buf[i + 2] * 255 + a / 2) / a);
	}

	*wp = (int)w;
	*hp = (int)h;
	return (buf);
}

/* Drop the fully transparent border, recording how much came off. */
static void
trim_sprite(Sprite *s, uint8_t *rgba, int w, int h)
{
	int x0 = w, y0 = h, x1 = -1, y1 = -1, x, y, i;
	uint8_t *out;
	BOOL opaque = YES;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (rgba[(y * w + x) * 4 + 3] == 0)
				continue;
			if (x < x0)
				x0 = x;
			if (x > x1)
				x1 = x;
			if (y < y0)
				y0 = y;
			if (y > y1)
				y1 = y;
		}
	}
	if (x1 < 0) {
		/* Nothing at all: keep a single transparent pixel. */
		x0 = y0 = 0;
		x1 = y1 = 0;
	}

	s.sourceWidth = w;
	s.sourceHeight = h;
	s.width = x1 - x0 + 1;
	s.height = y1 - y0 + 1;
	s.offsetX = x0;
	s.offsetY = h - 1 - y1;	/* rows are stored top down, sprites are not */

	if ((out = malloc((size_t)s.width * s.height * 4)) == NULL)
		err(1, NULL);
	for (y = 0; y < s.height; y++)
		memcpy(out + (size_t)y * s.width * 4,
		    rgba + ((size_t)(y + y0) * w + x0) * 4,
		    (size_t)s.width * 4);
	/*
	 * Opaque once the transparent border is gone, which is what lets
	 * SpriteKit draw the sprite without blending.
	 *
	 * ponytail: Apple decide this by rendering the trimmed image into an
	 * alpha-only context and scanning it, and a sprite whose content is
	 * a single pixel thin can come back not-opaque there even though
	 * every pixel it writes to the page is 255.  Reading the pixels we
	 * already have disagrees with them in that one case.
	 */
	for (i = 0; i < s.width * s.height; i++) {
		if (out[i * 4 + 3] != 255) {
			opaque = NO;
			break;
		}
	}
	s.rgba = out;
	s.opaque = opaque;
	free(rgba);
}

/* ------------------------------------------------------------------ */
/* Packing.                                                            */
/* ------------------------------------------------------------------ */

/*
 * Largest first, which is what makes MaxRects behave.  The area of the whole
 * source image decides -- not of what survived trimming, since the order is
 * settled before anything is cut away.  std::sort is not stable, so equal
 * areas come out in whatever order the partitioning leaves them; that order
 * reaches the atlas, which is why introsort.c spells the algorithm out.
 */
static bool
larger_source(const void *a, const void *b)
{
	Sprite *x = (__bridge Sprite *)a, *y = (__bridge Sprite *)b;

	return ((long)x.sourceWidth * x.sourceHeight >
	    (long)y.sourceWidth * y.sourceHeight);
}

static NSArray<Sprite *> *
sorted_sprites(NSArray<Sprite *> *in)
{
	NSUInteger n = in.count, i;
	const void **v;
	NSMutableArray *out;

	if (n < 2)
		return (in);
	if ((v = calloc(n, sizeof(*v))) == NULL)
		err(1, NULL);
	for (i = 0; i < n; i++)
		v[i] = (__bridge const void *)in[i];
	introsort((void **)v, n, larger_source);
	out = [NSMutableArray arrayWithCapacity:n];
	for (i = 0; i < n; i++)
		[out addObject:(__bridge Sprite *)v[i]];
	free(v);
	return (out);
}

/*
 * Pack every sprite, splitting into further pages only once a single page
 * has grown to the maximum texture dimension and still will not hold them.
 *
 * The page size is guessed from the total area at an assumed occupancy that
 * starts at 1.0 and gives up two percentage points on every failed attempt,
 * which is how Apple's packer converges: a perfect packing is tried first
 * and the bin loosened until everything fits.
 */
static int
pack_group(NSArray<Sprite *> *sprites, int *dims, int maxpages)
{
	NSMutableArray<Sprite *> *rest = [NSMutableArray arrayWithArray:sprites];
	int page = 0;

	while (rest.count > 0) {
		double area = 0, occupancy = 1.0;
		int maxdim = 0, dim;
		BOOL done = NO;

		if (page >= maxpages) {
			atlas_error(@"cannot fit input texture into a maximum "
			    "supported dimension of %d x %d.",
			    opt_maxdim, opt_maxdim);
			return (-1);
		}
		for (Sprite *s in rest) {
			int w = s.width + 2, h = s.height + 2;

			area += (double)w * h;
			if (w > maxdim)
				maxdim = w;
			if (h > maxdim)
				maxdim = h;
		}

		/*
		 * Guess the page from the area at an assumed occupancy, which
		 * starts at a perfect packing and gives up two percentage
		 * points every time the guess turns out to be too tight.  The
		 * same concession lifts a guess that is smaller than the
		 * largest sprite, which could never hold it.
		 */
		for (;;) {
			struct maxrects mr;
			NSMutableArray<Sprite *> *left;
			BOOL all = YES;

			dim = (int)ceil(sqrt(area / occupancy));
			if (opt_pot) {
				int pot = 1;

				while (pot < dim)
					pot *= 2;
				dim = pot;
			}
			if (dim >= maxdim) {
				BOOL last = (dim >= opt_maxdim);

				if (last)
					dim = opt_maxdim;
				mr_init(&mr, dim, dim);
				left = [NSMutableArray array];
				for (Sprite *s in rest) {
					struct mr_rect r;
					bool rot;

					if (!mr_insert(&mr, s.width + 2,
					    s.height + 2, &r, &rot)) {
						all = NO;
						if (!last)
							break;
						[left addObject:s];
						continue;
					}
					s.page = page;
					s.placed = r;
					s.rotated = rot;
				}
				if (all || (last && left.count < rest.count)) {
					dims[page] = dim;
					mr_free(&mr);
					rest = all ? [NSMutableArray array] :
					    left;
					done = YES;
					break;
				}
				mr_free(&mr);
				if (last) {
					atlas_error(@"cannot fit input texture "
					    "into a maximum supported "
					    "dimension of %d x %d.",
					    opt_maxdim, opt_maxdim);
					return (-1);
				}
			}
			if (occupancy <= 0.01)
				break;
			occupancy = fmax(occupancy - 0.02, 0.01);
		}
		if (!done)
			return (-1);
		page++;
	}
	return (page);
}

/* ------------------------------------------------------------------ */
/* Blitting.                                                           */
/* ------------------------------------------------------------------ */

static inline void
put(uint8_t *dst, int stride, int x, int y, const uint8_t *px)
{
	memcpy(dst + (size_t)y * stride + (size_t)x * 4, px, 4);
}

/*
 * Copy one sprite into the page, rotating it a quarter turn clockwise when
 * the packer asked for that, and repeat the outermost pixels into the one
 * pixel skirt the placement reserved.
 */
static void
blit_sprite(Sprite *s, uint8_t *page, int pw, int ph)
{
	int stride = pw * 4;
	int w = s.rotated ? s.height : s.width;
	int h = s.rotated ? s.width : s.height;
	int px = s.placed.x + 1;
	/*
	 * The packer counts rows from the top of the bin and the page counts
	 * them from the bottom, so a placement lands as far from the bottom
	 * of the page as the packer put it from the top of the bin.
	 */
	int py = ph - (s.placed.y + 1) - h;
	int x, y;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			const uint8_t *src;

			if (s.rotated)
				src = s.rgba +
				    ((size_t)(s.height - 1 - x) * s.width +
				    y) * 4;
			else
				src = s.rgba + ((size_t)y * s.width + x) * 4;
			put(page, stride, px + x, py + y, src);
		}
	}
	/*
	 * Only an untrimmed sprite gets the skirt.  A trimmed one already
	 * had transparent pixels around its content in the source, and
	 * repeating its new edge would put colour where the artwork says
	 * there is none.
	 */
	if (s.width != s.sourceWidth || s.height != s.sourceHeight)
		return;

	note(@"Extrude texture: %s", [s.name UTF8String]);
	for (x = 0; x < w; x++) {
		put(page, stride, px + x, py - 1,
		    page + (size_t)py * stride + (size_t)(px + x) * 4);
		put(page, stride, px + x, py + h,
		    page + (size_t)(py + h - 1) * stride +
		    (size_t)(px + x) * 4);
	}
	for (y = -1; y <= h; y++) {
		put(page, stride, px - 1, py + y,
		    page + (size_t)(py + y) * stride + (size_t)px * 4);
		put(page, stride, px + w, py + y,
		    page + (size_t)(py + y) * stride +
		    (size_t)(px + w - 1) * 4);
	}
}

/* ------------------------------------------------------------------ */
/* Writing.                                                            */
/* ------------------------------------------------------------------ */

static BOOL
write_png(NSURL *url, const uint8_t *rgba, int w, int h)
{
	CGColorSpaceRef cs;
	CGContextRef ctx;
	CGImageRef img;
	CGImageDestinationRef dst;
	uint8_t *pm;
	size_t i, n = (size_t)w * h * 4;
	BOOL ok;

	/* CoreGraphics wants premultiplied data; PNG stores straight. */
	if ((pm = malloc(n)) == NULL)
		err(1, NULL);
	memcpy(pm, rgba, n);
	for (i = 0; i < n; i += 4) {
		unsigned a = pm[i + 3];

		if (a == 255)
			continue;
		pm[i + 0] = (uint8_t)((pm[i + 0] * a + 127) / 255);
		pm[i + 1] = (uint8_t)((pm[i + 1] * a + 127) / 255);
		pm[i + 2] = (uint8_t)((pm[i + 2] * a + 127) / 255);
	}

	cs = CGColorSpaceCreateDeviceRGB();
	ctx = CGBitmapContextCreate(pm, w, h, 8, (size_t)w * 4, cs,
	    kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
	CGColorSpaceRelease(cs);
	if (ctx == NULL) {
		free(pm);
		return (NO);
	}
	img = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);
	if (img == NULL) {
		free(pm);
		return (NO);
	}
	dst = CGImageDestinationCreateWithURL((__bridge CFURLRef)url,
	    CFSTR("public.png"), 1, NULL);
	if (dst == NULL) {
		CGImageRelease(img);
		free(pm);
		return (NO);
	}
	CGImageDestinationAddImage(dst, img, NULL);
	ok = CGImageDestinationFinalize(dst);
	CFRelease(dst);
	CGImageRelease(img);
	free(pm);
	return (ok);
}

/*
 * The legacy PVRTexTool container, which is what SpriteKit reads.  Its rows
 * run bottom up, its colour is premultiplied by alpha, and its payload is
 * rounded up to an eight byte boundary.
 */
static NSData *
build_pvr(const uint8_t *rgba, int w, int h, int fmt)
{
	NSMutableData *out;
	uint32_t hdr[13], flags, masks[4];
	size_t n = (size_t)w * h, len, pad, i;
	int bpp, x, y;

	switch (fmt) {
	case FMT_PVR_RGBA8888:
		flags = 0x12;
		bpp = 4;
		masks[0] = 0xff000000; masks[1] = 0x00ff0000;
		masks[2] = 0x0000ff00; masks[3] = 0x000000ff;
		break;
	case FMT_PVR_RGBA4444:
		flags = 0x10;
		bpp = 2;
		masks[0] = 0x0000f000; masks[1] = 0x00000f00;
		masks[2] = 0x000000f0; masks[3] = 0x0000000f;
		break;
	case FMT_PVR_RGBA5551:
		flags = 0x11;
		bpp = 2;
		masks[0] = 0x0000f800; masks[1] = 0x000007c0;
		masks[2] = 0x0000003e; masks[3] = 0x00000001;
		break;
	case FMT_PVR_RGB565:
		flags = 0x13;
		bpp = 2;
		masks[0] = 0x0000f800; masks[1] = 0x000007e0;
		masks[2] = 0x0000001f; masks[3] = 0x00000000;
		break;
	default:
		return (nil);
	}

	/*
	 * The pixel count is rounded up to a multiple of four.
	 *
	 * ponytail: Apple fill those few trailing pixels with whatever
	 * follows their page buffer in memory -- a read past its end -- so a
	 * page whose area is not a multiple of four differs from theirs in
	 * up to three pixels no reader looks at.  Zero is what belongs there.
	 */
	len = n * bpp;
	pad = ((n + 3) & ~(size_t)3) * bpp - len;

	hdr[0] = 52;
	hdr[1] = (uint32_t)h;
	hdr[2] = (uint32_t)w;
	hdr[3] = 0;
	hdr[4] = flags;
	hdr[5] = (uint32_t)(len + pad);
	hdr[6] = (uint32_t)bpp;
	hdr[7] = masks[0];
	hdr[8] = masks[1];
	hdr[9] = masks[2];
	hdr[10] = masks[3];
	hdr[11] = 0x21525650;	/* 'PVR!' */
	hdr[12] = 0;

	out = [NSMutableData dataWithBytes:hdr length:sizeof(hdr)];
	for (y = h - 1; y >= 0; y--) {
		for (x = 0; x < w; x++) {
			const uint8_t *p = rgba + ((size_t)y * w + x) * 4;
			unsigned a = p[3];
			unsigned r = (p[0] * a + 127) / 255;
			unsigned g = (p[1] * a + 127) / 255;
			unsigned bl = (p[2] * a + 127) / 255;
			uint8_t q[4];
			uint16_t v;

			if (fmt == FMT_PVR_RGBA8888) {
				q[0] = (uint8_t)r;
				q[1] = (uint8_t)g;
				q[2] = (uint8_t)bl;
				q[3] = (uint8_t)a;
				[out appendBytes:q length:sizeof(q)];
				continue;
			}
			switch (fmt) {
			case FMT_PVR_RGBA4444:
				v = (uint16_t)(((r >> 4) << 12) |
				    ((g >> 4) << 8) | ((bl >> 4) << 4) |
				    (a >> 4));
				break;
			case FMT_PVR_RGBA5551:
				v = (uint16_t)(((r >> 3) << 11) |
				    ((g >> 3) << 6) | ((bl >> 3) << 1) |
				    (a >> 7));
				break;
			default:
				v = (uint16_t)(((r >> 3) << 11) |
				    ((g >> 2) << 5) | (bl >> 3));
				break;
			}
			[out appendBytes:&v length:sizeof(v)];
		}
	}
	for (i = 0; i < pad; i++) {
		uint8_t z = 0;

		[out appendBytes:&z length:1];
	}
	return (out);
}

static NSData *
gzip(NSData *in)
{
	z_stream z;
	NSMutableData *out = [NSMutableData data];
	uint8_t buf[65536];

	memset(&z, 0, sizeof(z));
	/* Level 9, which is what Apple's XFL byte says they asked for. */
	if (deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS,
	    8, Z_DEFAULT_STRATEGY) != Z_OK)
		return (nil);
	z.next_in = (Bytef *)[in bytes];
	z.avail_in = (uInt)[in length];
	do {
		z.next_out = buf;
		z.avail_out = sizeof(buf);
		if (deflate(&z, Z_FINISH) == Z_STREAM_ERROR) {
			deflateEnd(&z);
			return (nil);
		}
		[out appendBytes:buf length:sizeof(buf) - z.avail_out];
	} while (z.avail_out == 0);
	deflateEnd(&z);
	return (out);
}

/* ------------------------------------------------------------------ */

static NSString *
suffix_of(NSString *base)
{
	NSString *best = @"";
	size_t i;

	for (i = 0; i < kSuffixCount; i++) {
		if (![base hasSuffix:kSuffixes[i]])
			continue;
		if (kSuffixes[i].length > best.length)
			best = kSuffixes[i];
	}
	return (best);
}

static void
usage(void)
{
	printf("Usage: %s [-c][-p][-g][-v][-f format][-s size] "
	    "input_textures_folder optional_output_folder\n", progname);
	printf("\t -c: use copy textures instead of generating texture "
	    "atlas.\n");
	printf("\t -p: force texture atlas to have power of two "
	    "dimensions.\n");
	printf("\t -g: disable warnings during texture atlas generation.");
	printf("\t -v: enable verbose output.\n");
	printf("\t -f format: set texture output format.\n");
	printf("\t\t format = %d (default): output RGBA8888 format.\n",
	    FMT_RGBA8888);
	printf("\t\t format = %d: output compressed RGBA8888 format.\n",
	    FMT_PVR_RGBA8888);
	printf("\t\t format = %d: output compressed RGBA4444 format.\n",
	    FMT_PVR_RGBA4444);
	printf("\t\t format = %d: output compressed RGBA5551 format.\n",
	    FMT_PVR_RGBA5551);
	printf("\t\t format = %d: output compressed RGB565 format.\n",
	    FMT_PVR_RGB565);
	printf("\t -s size: set maximum output texture dimension.\n");
	printf("\t\t size = 1 (default): %lu x %lu.\n", 2048UL, 2048UL);
	printf("\t\t size = 2: 4096 x 4096.\n");
}

int
main(int argc, char *argv[])
{
@autoreleasepool {
	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *indir, *outdir, *atlasName, *bundle;
	NSMutableArray<NSString *> *files = [NSMutableArray array];
	NSMutableDictionary<NSString *, NSMutableArray<Sprite *> *> *groups;
	NSMutableArray *images = [NSMutableArray array];
	NSArray<NSString *> *groupKeys;
	BOOL isdir = NO;
	int ch;

	progname = argv[0];
	while ((ch = getopt(argc, argv, "cpgvf:s:")) != -1) {
		switch (ch) {
		case 'c':
			opt_copy = true;
			break;
		case 'p':
			opt_pot = true;
			break;
		case 'g':
			opt_quiet = true;
			break;
		case 'v':
			opt_verbose = true;
			break;
		case 'f':
			opt_format = atoi(optarg);
			if (opt_format < 1 || opt_format > FMT_MAX) {
				atlas_error(@"Invalid output format "
				    "specified.");
				return (1);
			}
			break;
		case 's':
			switch (atoi(optarg)) {
			case 1:
				opt_maxdim = 2048;
				break;
			case 2:
				opt_maxdim = 4096;
				break;
			default:
				atlas_error(@"Invalid maximum output size "
				    "specified.");
				return (1);
			}
			break;
		default:
			usage();
			return (-1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		return (-1);
	}

	indir = [[NSString stringWithUTF8String:argv[0]]
	    stringByStandardizingPath];
	if (![fm fileExistsAtPath:indir isDirectory:&isdir] || !isdir) {
		atlas_error(@"input textures folder is nil.");
		return (1);
	}
	outdir = argc > 1 ? [[NSString stringWithUTF8String:argv[1]]
	    stringByStandardizingPath] :
	    [indir stringByDeletingLastPathComponent];

	if (opt_copy) {
		NSString *dst = [outdir stringByAppendingPathComponent:
		    [indir lastPathComponent]];

		[fm removeItemAtPath:dst error:NULL];
		if (![fm copyItemAtPath:indir toPath:dst error:NULL]) {
			atlas_error(@"failed to create texture atlas bundle "
			    "'%@'.", dst);
			return (1);
		}
		return (0);
	}

	atlasName = [indir lastPathComponent];
	bundle = [outdir stringByAppendingPathComponent:
	    [atlasName stringByAppendingPathExtension:@"atlasc"]];

	/*
	 * Collect the images in the order the directory lists them, not in
	 * name order.  Equal-sized sprites keep that order all the way into
	 * the atlas, so the listing is part of the output.
	 */
	for (NSURL *u in [fm enumeratorAtURL:[NSURL fileURLWithPath:indir]
	    includingPropertiesForKeys:nil
	    options:NSDirectoryEnumerationSkipsHiddenFiles |
	    NSDirectoryEnumerationSkipsSubdirectoryDescendants
	    errorHandler:nil]) {
		NSString *e = [u lastPathComponent];
		NSString *lower = [e lowercaseString];

		if ([lower hasSuffix:@".png"] || [lower hasSuffix:@".jpg"])
			[files addObject:e];
	}
	if (files.count == 0) {
		atlas_error(@"input textures folder is nil.");
		return (1);
	}

	groups = [NSMutableDictionary dictionary];
	for (NSString *f in files) {
		NSURL *url = [NSURL fileURLWithPath:
		    [indir stringByAppendingPathComponent:f]];
		NSString *base = [f stringByDeletingPathExtension];
		NSString *sfx = suffix_of(base);
		Sprite *s;
		uint8_t *rgba;
		int w, h;

		note(@"Loading texture file: '%@'.", [url absoluteString]);
		if ((rgba = decode_image(url, &w, &h)) == NULL) {
			atlas_error(@"Error loading image file '%s'",
			    [f UTF8String]);
			return (1);
		}
		s = [Sprite new];
		s.name = f;
		trim_sprite(s, rgba, w, h);
		if (groups[sfx] == nil)
			groups[sfx] = [NSMutableArray array];
		[groups[sfx] addObject:s];
	}

	if (![fm fileExistsAtPath:bundle] &&
	    ![fm createDirectoryAtPath:bundle withIntermediateDirectories:YES
	      attributes:nil error:NULL]) {
		atlas_error(@"failed to create texture atlas bundle '%@'.",
		    bundle);
		return (1);
	}

	/*
	 * ponytail: the groups are visited in the order the directory first
	 * mentioned each suffix.  Apple walk their own dictionary, whose
	 * order is neither that nor the suffix table's, so an atlas that
	 * mixes device suffixes lists the same pages -- each of them byte
	 * for byte ours -- in a different order inside the plist.
	 */
	groupKeys = [groups allKeys];
	for (NSString *sfx in groupKeys) {
		NSArray<Sprite *> *sprites = sorted_sprites(groups[sfx]);
		int dims[64], npages, p;

		npages = pack_group(sprites, dims,
		    (int)(sizeof(dims) / sizeof(dims[0])));
		if (npages < 0)
			return (1);
		if (npages > 1)
			note(@"Splitting '%@' into %d texture atlases due to "
			    "input texture dimensions.", atlasName, npages);

		for (p = 0; p < npages; p++) {
			NSMutableArray *subs = [NSMutableArray array];
			NSString *pagename, *ext;
			NSURL *pageurl;
			uint8_t *page;
			int pw = 0, ph = 0;

			for (Sprite *s in sprites) {
				int rx, ry;

				if (s.page != p)
					continue;
				rx = s.placed.x + s.placed.w;
				ry = s.placed.y + s.placed.h;
				if (rx > pw)
					pw = rx;
				if (ry > ph)
					ph = ry;
			}
			if (opt_pot)
				pw = ph = dims[p];
			note(@"Generated texture atlas size [%d, %d].", pw, ph);

			if ((page = calloc(1, (size_t)pw * ph * 4)) == NULL)
				err(1, NULL);
			for (Sprite *s in sprites) {
				if (s.page == p)
					blit_sprite(s, page, pw, ph);
			}

			for (Sprite *s in sprites) {
				int tw, th, tx, ty;

				if (s.page != p)
					continue;
				tw = s.rotated ? s.height : s.width;
				th = s.rotated ? s.width : s.height;
				tx = s.placed.x + 1;
				ty = ph - (s.placed.y + 1) - th;
				/*
				 * Small dictionaries enumerate in insertion
				 * order, and the binary plist writer stores
				 * keys in the order it enumerates them, so
				 * this is also the order they appear on disk.
				 */
				NSMutableDictionary *sub =
				    [NSMutableDictionary dictionary];

				sub[@"name"] = s.name;
				/* A fresh array each time: the plist writer
				 * emits one object per instance, and Apple's
				 * file carries an empty array per subimage. */
				sub[@"aliases"] = [NSMutableArray array];
				sub[@"isFullyOpaque"] =
				    [NSNumber numberWithBool:s.opaque];
				sub[@"textureRect"] = [NSString
				    stringWithFormat:@"{{%d, %d}, {%d, %d}}",
				    tx, ty, tw, th];
				sub[@"spriteOffset"] = [NSString
				    stringWithFormat:@"{%d, %d}",
				    s.offsetX, s.offsetY];
				sub[@"textureRotated"] =
				    [NSNumber numberWithBool:s.rotated];
				sub[@"spriteSourceSize"] = [NSString
				    stringWithFormat:@"{%d, %d}",
				    s.sourceWidth, s.sourceHeight];
				[subs addObject:sub];
			}

			ext = opt_format == FMT_RGBA8888 ? @"png" : @"pvr.gz";
			pagename = [NSString stringWithFormat:@"%@.%d%@.%@",
			    atlasName, p + 1, sfx, ext];
			pageurl = [NSURL fileURLWithPath:
			    [bundle stringByAppendingPathComponent:pagename]];

			note(@"Writing texture atlas '%@' file.",
			    [bundle stringByAppendingPathComponent:pagename]);
			if (opt_format == FMT_RGBA8888) {
				if (!write_png(pageurl, page, pw, ph)) {
					atlas_error(@"Failed to write image "
					    "to %@", pageurl);
					free(page);
					return (1);
				}
			} else {
				NSData *d = gzip(build_pvr(page, pw, ph,
				    opt_format));

				if (d == nil ||
				    ![d writeToURL:pageurl atomically:NO]) {
					atlas_error(@"Failed to write image "
					    "to %@", pageurl);
					free(page);
					return (1);
				}
			}
			free(page);

			{
				NSMutableDictionary *im =
				    [NSMutableDictionary dictionary];

				im[@"path"] = pagename;
				im[@"size"] = [NSString stringWithFormat:
				    @"{%d, %d}", pw, ph];
				im[@"subimages"] = subs;
				[images addObject:im];
			}
		}
	}

	{
		NSMutableDictionary *plist = [NSMutableDictionary dictionary];
		NSString *path = [bundle stringByAppendingPathComponent:
		    [atlasName stringByAppendingPathExtension:@"plist"]];
		NSData *d;

		plist[@"version"] = @1;
		plist[@"format"] = @"APPL";
		plist[@"images"] = images;
		d = [NSPropertyListSerialization
		    dataWithPropertyList:plist
		    format:NSPropertyListBinaryFormat_v1_0 options:0
		    error:NULL];

		note(@"Writing texture atlas plist '%@' file.", path);
		if (d == nil || ![d writeToFile:path atomically:NO]) {
			atlas_error(@"cannot create plist file '%@'.", path);
			return (1);
		}
	}
	return (0);
}
}
