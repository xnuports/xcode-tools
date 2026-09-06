/*
 * bkeyed.c -- see bkeyed.h.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "bkeyed.h"

#include <stdlib.h>
#include <string.h>

struct reader {
	const unsigned char	*b;
	size_t			 len;
	const unsigned char	*offsets;
	size_t			 nobjects;
	uint8_t			 offset_size, ref_size;
};

static struct bk_value	*read_object(struct reader *r, size_t index);

static uint64_t
be(const unsigned char *p, size_t n)
{
	uint64_t v = 0;
	size_t i;

	for (i = 0; i < n; i++)
		v = (v << 8) | p[i];
	return (v);
}

static struct bk_value *
alloc(enum bk_kind kind)
{
	struct bk_value *v = calloc(1, sizeof(*v));

	if (v != NULL)
		v->kind = kind;
	return (v);
}

static struct bk_value *
read_object(struct reader *r, size_t index)
{
	size_t pos, count, i;
	unsigned char marker;
	unsigned nibble;
	struct bk_value *v;

	if (index >= r->nobjects)
		return (NULL);

	pos = (size_t)be(r->offsets + index * r->offset_size, r->offset_size);
	if (pos >= r->len)
		return (NULL);

	marker = r->b[pos++];
	nibble = marker & 0x0f;

	switch (marker & 0xf0) {
	case 0x00:
		if (marker == 0x08 || marker == 0x09) {
			v = alloc(BK_BOOL);
			if (v != NULL)
				v->integer = (marker == 0x09);
			return (v);
		}
		return (alloc(BK_NULL));

	case 0x10: {			/* integer */
		size_t n = (size_t)1 << nibble;

		if (pos + n > r->len)
			return (NULL);
		v = alloc(BK_INT);
		if (v != NULL)
			v->integer = (int64_t)be(r->b + pos, n);
		return (v);
	}

	case 0x20: {			/* real */
		size_t n = (size_t)1 << nibble;

		if (pos + n > r->len)
			return (NULL);
		v = alloc(BK_REAL);
		if (v == NULL)
			return (NULL);
		if (n == 4) {
			uint32_t bits = (uint32_t)be(r->b + pos, 4);
			float f;

			memcpy(&f, &bits, 4);
			v->real = f;
		} else if (n == 8) {
			uint64_t bits = be(r->b + pos, 8);
			double d;

			memcpy(&d, &bits, 8);
			v->real = d;
		}
		return (v);
	}

	case 0x40:			/* data */
	case 0x50:			/* ASCII string */
	case 0x60: {			/* UTF-16 string */
		count = nibble;
		if (nibble == 0x0f) {
			unsigned char m2;
			size_t n;

			if (pos >= r->len)
				return (NULL);
			m2 = r->b[pos++];
			n = (size_t)1 << (m2 & 0x0f);
			if (pos + n > r->len)
				return (NULL);
			count = (size_t)be(r->b + pos, n);
			pos += n;
		}

		if ((marker & 0xf0) == 0x40) {
			if (pos + count > r->len)
				return (NULL);
			v = alloc(BK_DATA);
			if (v == NULL)
				return (NULL);
			v->data = malloc(count ? count : 1);
			if (v->data == NULL) {
				free(v);
				return (NULL);
			}
			memcpy(v->data, r->b + pos, count);
			v->length = count;
			return (v);
		}

		v = alloc(BK_STRING);
		if (v == NULL)
			return (NULL);

		if ((marker & 0xf0) == 0x50) {
			if (pos + count > r->len) {
				free(v);
				return (NULL);
			}
			v->string = malloc(count + 1);
			if (v->string == NULL) {
				free(v);
				return (NULL);
			}
			memcpy(v->string, r->b + pos, count);
			v->string[count] = '\0';
		} else {
			/* UTF-16BE; only the ASCII range appears here. */
			size_t k;

			if (pos + count * 2 > r->len) {
				free(v);
				return (NULL);
			}
			v->string = malloc(count + 1);
			if (v->string == NULL) {
				free(v);
				return (NULL);
			}
			for (k = 0; k < count; k++)
				v->string[k] = (char)r->b[pos + k * 2 + 1];
			v->string[count] = '\0';
		}
		return (v);
	}

	case 0x80: {			/* UID */
		size_t n = nibble + 1;

		if (pos + n > r->len)
			return (NULL);
		v = alloc(BK_UID);
		if (v != NULL)
			v->integer = (int64_t)be(r->b + pos, n);
		return (v);
	}

	case 0xa0: {			/* array */
		count = nibble;
		if (nibble == 0x0f) {
			unsigned char m2 = r->b[pos++];
			size_t n = (size_t)1 << (m2 & 0x0f);

			count = (size_t)be(r->b + pos, n);
			pos += n;
		}
		if (pos + count * r->ref_size > r->len)
			return (NULL);

		v = alloc(BK_ARRAY);
		if (v == NULL)
			return (NULL);
		v->items = calloc(count ? count : 1, sizeof(*v->items));
		if (v->items == NULL) {
			free(v);
			return (NULL);
		}
		v->length = count;
		for (i = 0; i < count; i++)
			v->items[i] = read_object(r, (size_t)be(r->b + pos +
			    i * r->ref_size, r->ref_size));
		return (v);
	}

	case 0xd0: {			/* dictionary */
		count = nibble;
		if (nibble == 0x0f) {
			unsigned char m2 = r->b[pos++];
			size_t n = (size_t)1 << (m2 & 0x0f);

			count = (size_t)be(r->b + pos, n);
			pos += n;
		}
		if (pos + count * 2 * r->ref_size > r->len)
			return (NULL);

		v = alloc(BK_DICT);
		if (v == NULL)
			return (NULL);
		v->pairs = calloc(count ? count : 1, sizeof(*v->pairs));
		if (v->pairs == NULL) {
			free(v);
			return (NULL);
		}
		v->length = count;
		for (i = 0; i < count; i++) {
			v->pairs[i].key = read_object(r, (size_t)be(r->b +
			    pos + i * r->ref_size, r->ref_size));
			v->pairs[i].value = read_object(r, (size_t)be(r->b +
			    pos + (count + i) * r->ref_size, r->ref_size));
		}
		return (v);
	}
	}

	return (NULL);
}

struct bk_value *
bk_parse(const unsigned char *bytes, size_t len)
{
	struct reader r;
	const unsigned char *trailer;
	size_t root;

	if (len < 40 || memcmp(bytes, "bplist00", 8) != 0)
		return (NULL);

	trailer = bytes + len - 32;
	r.b = bytes;
	r.len = len;
	r.offset_size = trailer[6];
	r.ref_size = trailer[7];
	r.nobjects = (size_t)be(trailer + 8, 8);
	root = (size_t)be(trailer + 16, 8);
	r.offsets = bytes + (size_t)be(trailer + 24, 8);

	if (r.offset_size == 0 || r.ref_size == 0 ||
	    (size_t)(r.offsets - bytes) + r.nobjects * r.offset_size > len)
		return (NULL);

	return (read_object(&r, root));
}

void
bk_free(struct bk_value *v)
{
	size_t i;

	if (v == NULL)
		return;
	for (i = 0; i < v->length && v->items != NULL; i++)
		bk_free(v->items[i]);
	for (i = 0; i < v->length && v->pairs != NULL; i++) {
		bk_free(v->pairs[i].key);
		bk_free(v->pairs[i].value);
	}
	free(v->items);
	free(v->pairs);
	free(v->string);
	free(v->data);
	free(v);
}

const struct bk_value *
bk_dict_get(const struct bk_value *d, const char *key)
{
	size_t i;

	if (d == NULL || d->kind != BK_DICT)
		return (NULL);
	for (i = 0; i < d->length; i++)
		if (d->pairs[i].key != NULL &&
		    d->pairs[i].key->kind == BK_STRING &&
		    strcmp(d->pairs[i].key->string, key) == 0)
			return (d->pairs[i].value);
	return (NULL);
}
