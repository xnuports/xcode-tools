/*
 * ktx2.c -- writing Khronos texture container version 2.
 *
 * Version 2 keeps the same payload as version 1 and rearranges everything
 * around it: a fixed header, an index of byte offsets, a data format
 * descriptor that spells out what a block holds, the key/value block, and
 * then the levels -- smallest first, each aligned to the size of a texel
 * block.
 *
 * Apple's tool reaches this only through the output file's extension.
 * --file_format=KTX2 does not select it, which is worth knowing before
 * anyone goes looking: give their tool that flag and an output path ending
 * in anything else and it writes version 1.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "ktx2.h"

static const uint8_t ktx2_id[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

struct buf {
	uint8_t	*p;
	size_t	 len, cap;
	int	 failed;
};

static void
put(struct buf *b, const void *bytes, size_t n)
{
	if (b->failed)
		return;
	if (b->len + n > b->cap) {
		size_t want = (b->len + n) * 2 + 256;
		uint8_t *np = realloc(b->p, want);

		if (np == NULL) {
			b->failed = 1;
			return;
		}
		b->p = np;
		b->cap = want;
	}
	memcpy(b->p + b->len, bytes, n);
	b->len += n;
}

static void
put32(struct buf *b, uint32_t v)
{
	uint8_t x[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
	    (uint8_t)(v >> 24) };

	put(b, x, sizeof(x));
}

static void
put64(struct buf *b, uint64_t v)
{
	put32(b, (uint32_t)v);
	put32(b, (uint32_t)(v >> 32));
}

static void
pad_to(struct buf *b, size_t align)
{
	static const uint8_t zero[16] = { 0 };

	while (b->len % align != 0 && !b->failed)
		put(b, zero, 1);
}

/*
 * The key/value block.  Version 2 wants the entries sorted by key, which is
 * the one place it differs from version 1's insertion order, and it wants a
 * KTXwriter whether or not anything is being annotated -- Apple write
 * "Unidentified app / libktx v4.0" when --disable_annotation asks for no
 * annotation, rather than leaving the key out.
 */
static void
put_kv(struct buf *b, const char *key, const char *value)
{
	size_t klen = strlen(key) + 1, vlen = strlen(value) + 1;

	put32(b, (uint32_t)(klen + vlen));
	put(b, key, klen);
	put(b, value, vlen);
	pad_to(b, 4);
}

uint8_t *
ktx2_write(void **levels, const size_t *sizes, const int *widths,
    const int *heights, const int *depths, int nlevels, int faces, _Bool array,
    uint32_t vk_format, int block_bytes,
    int block_x, int block_y, int type_size, const struct format_dfd *dfd,
    bool premultiplied, bool srgb, const char *writer, const char *options,
    const char *version, size_t *out_len)
{
	struct buf b = { NULL, 0, 0, 0 };
	struct buf kv = { NULL, 0, 0, 0 };
	size_t dfd_off, dfd_len, kv_off, index_off, data_off;
	size_t align = (size_t)block_bytes;
	size_t off;
	int i;

	if (nlevels <= 0 || faces < 1 || dfd == NULL)
		return (NULL);

	/* Sorted by key: KTXwriter, then TC_Options, then TC_Version. */
	put_kv(&kv, "KTXwriter", writer);
	if (options != NULL)
		put_kv(&kv, "TC_Options", options);
	if (version != NULL)
		put_kv(&kv, "TC_Version", version);
	if (kv.failed) {
		free(kv.p);
		return (NULL);
	}

	/*
	 * Levels start at a multiple of the least common multiple of the
	 * texel block size and four.  Four alone is not enough: a three byte
	 * texel wants twelve, and only the uncompressed formats have one.
	 */
	{
		size_t x = align < 1 ? 1 : align, y = 4, t;

		while (y != 0) {
			t = x % y;
			x = y;
			y = t;
		}
		align = (align < 1 ? 1 : (size_t)align) * 4 / x;
	}

	dfd_len = 4 + 24 + (size_t)dfd->nsamples * 16;
	index_off = 12 + 36 + 32;
	dfd_off = index_off + (size_t)nlevels * 24;
	kv_off = dfd_off + dfd_len;
	data_off = kv_off + kv.len;
	data_off = (data_off + align - 1) / align * align;

	put(&b, ktx2_id, sizeof(ktx2_id));
	put32(&b, vk_format);
	put32(&b, (uint32_t)type_size);
	put32(&b, (uint32_t)widths[0]);
	put32(&b, (uint32_t)heights[0]);
	/* A volume says how deep it is; everything else says nothing. */
	put32(&b, depths != NULL && depths[0] > 1 ?
	    (uint32_t)depths[0] : 0);
	/*
	 * An array's elements are layers; a cubemap's sides are faces.  The
	 * level data is laid out the same way either way, one level of all
	 * of them at a time, so only these two words tell them apart.
	 */
	put32(&b, array ? (uint32_t)faces : 0);	/* layerCount */
	put32(&b, array ? 1 : (uint32_t)faces);	/* faceCount */
	put32(&b, (uint32_t)nlevels);
	put32(&b, 0);				/* supercompressionScheme */
	put32(&b, (uint32_t)dfd_off);
	put32(&b, (uint32_t)dfd_len);
	put32(&b, (uint32_t)kv_off);
	put32(&b, (uint32_t)kv.len);
	put64(&b, 0);				/* sgdByteOffset */
	put64(&b, 0);				/* sgdByteLength */

	/*
	 * The index runs level 0 first, as version 1 does, but the levels it
	 * points at are stored the other way round -- smallest at the lowest
	 * offset -- so a reader can take the small ones without seeking past
	 * the large.  So the offsets in the index descend.  Every level
	 * starts on the same boundary the first one does, so a level whose
	 * bytes do not fill out to it is followed by padding the index does
	 * not count: byteLength stays the level's own size.
	 */
	for (i = 0; i < nlevels; i++) {
		size_t whole = sizes[i] * (size_t)faces;
		int j;

		off = data_off;
		for (j = nlevels - 1; j > i; j--)
			off += (sizes[j] * (size_t)faces + align - 1) /
			    align * align;
		put64(&b, off);
		put64(&b, whole);
		put64(&b, whole);		/* uncompressed: the same */
	}

	/* The descriptor. */
	put32(&b, (uint32_t)dfd_len);
	put32(&b, 0);				/* vendorId, descriptorType */
	put32(&b, 2 | ((uint32_t)(dfd_len - 4) << 16));
	{
		/*
		 * Version 1 says the colour has alpha folded into it with a
		 * key; version 2 has a bit in the descriptor for it, which
		 * is the only thing --alpha_mode=Premultiply changes in the
		 * whole file.
		 */
		uint8_t hdr[8] = {
			dfd->color_model,
			1,			/* colorPrimaries: BT709 */
			srgb ? 2 : 1,		/* transferFunction */
			premultiplied ? 1 : 0,	/* flags */
			(uint8_t)(block_x - 1), (uint8_t)(block_y - 1), 0, 0
		};

		put(&b, hdr, sizeof(hdr));
	}
	{
		uint8_t planes[8] = { (uint8_t)block_bytes, 0, 0, 0,
		    0, 0, 0, 0 };

		put(&b, planes, sizeof(planes));
	}
	for (i = 0; i < dfd->nsamples; i++) {
		const struct format_sample *s = &dfd->sample[i];
		uint8_t w[4] = { (uint8_t)s->bit_offset,
		    (uint8_t)(s->bit_offset >> 8), s->bit_length,
		    s->channel_type };

		put(&b, w, sizeof(w));
		put32(&b, 0);			/* samplePosition0..3 */
		put32(&b, s->lower);
		put32(&b, s->upper);
	}

	put(&b, kv.p, kv.len);
	free(kv.p);
	pad_to(&b, align);

	for (i = nlevels - 1; i >= 0; i--) {
		int j;

		for (j = 0; j < faces; j++)
			put(&b, levels[i * faces + j], sizes[i]);
		if (i > 0)
			pad_to(&b, align);
	}

	if (b.failed) {
		free(b.p);
		return (NULL);
	}
	*out_len = b.len;
	return (b.p);
}
