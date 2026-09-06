/*
 * resfork.c -- see resfork.h.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "resfork.h"

#include <sys/stat.h>
#include <sys/xattr.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint16_t
be16(const uint8_t *p)
{
	return ((uint16_t)((p[0] << 8) | p[1]));
}

static uint32_t
be32(const uint8_t *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

/* The data offset in a reference list entry is three bytes. */
static uint32_t
be24(const uint8_t *p)
{
	return (((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2]);
}

uint8_t *
resfork_read_file(const char *path, bool datafork, size_t *len)
{
	uint8_t *buf;
	ssize_t n;

	if (datafork) {
		struct stat st;
		int fd = open(path, O_RDONLY);

		if (fd < 0)
			return (NULL);
		if (fstat(fd, &st) != 0 || st.st_size < 0) {
			(void)close(fd);
			return (NULL);
		}
		buf = malloc((size_t)st.st_size + 1);
		if (buf == NULL) {
			(void)close(fd);
			return (NULL);
		}
		n = read(fd, buf, (size_t)st.st_size);
		(void)close(fd);
		if (n < 0) {
			free(buf);
			return (NULL);
		}
		*len = (size_t)n;
		return (buf);
	}

	{
		struct stat st;

		/* A file with no resource fork reads as an empty one; a file
		 * that is not there at all has to be told apart from that. */
		if (stat(path, &st) != 0)
			return (NULL);
	}

	n = getxattr(path, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0);
	if (n < 0)
		n = 0;

	buf = malloc((size_t)n + 1);
	if (buf == NULL)
		return (NULL);

	if (n > 0 && getxattr(path, XATTR_RESOURCEFORK_NAME, buf, (size_t)n,
	    0, 0) != n) {
		free(buf);
		return (NULL);
	}
	*len = (size_t)n;
	return (buf);
}

int
resfork_parse(const uint8_t *b, size_t len, struct resfork *out,
    const char **why)
{
	uint32_t data_off, map_off, data_len, map_len;
	size_t type_list, name_list, i, ntypes, total = 0, slot = 0;
	const uint8_t *map;

	out->items = NULL;
	out->count = 0;

	/* An empty fork is a file with no resources, not a broken one. */
	if (len == 0)
		return (0);

	if (len < 16) {
		*why = "resource fork is too short to hold a header";
		return (-1);
	}

	data_off = be32(b);
	map_off = be32(b + 4);
	data_len = be32(b + 8);
	map_len = be32(b + 12);

	if ((size_t)map_off + map_len > len || (size_t)data_off + data_len > len ||
	    map_len < 30) {
		*why = "resource fork header points outside the file";
		return (-1);
	}
	map = b + map_off;

	type_list = be16(map + 24);
	name_list = be16(map + 26);
	if (type_list + 2 > map_len) {
		*why = "type list lies outside the resource map";
		return (-1);
	}

	/* Both counts are stored one less than the true number. */
	ntypes = (size_t)be16(map + type_list) + 1;
	if (be16(map + type_list) == 0xffff)
		return (0);		/* the empty-map spelling */

	if (type_list + 2 + ntypes * 8 > map_len) {
		*why = "type list runs past the end of the resource map";
		return (-1);
	}

	/* One pass to count, so the array is allocated once. */
	for (i = 0; i < ntypes; i++) {
		const uint8_t *t = map + type_list + 2 + i * 8;

		total += (size_t)be16(t + 4) + 1;
	}

	out->items = calloc(total, sizeof(*out->items));
	if (out->items == NULL) {
		*why = "out of memory";
		return (-1);
	}

	for (i = 0; i < ntypes; i++) {
		const uint8_t *t = map + type_list + 2 + i * 8;
		size_t nres = (size_t)be16(t + 4) + 1;
		size_t ref_off = type_list + be16(t + 6);
		size_t j;

		if (ref_off + nres * 12 > map_len) {
			*why = "reference list runs past the end of the map";
			goto fail;
		}

		for (j = 0; j < nres; j++) {
			const uint8_t *r = map + ref_off + j * 12;
			struct resource *res = &out->items[slot];
			int16_t name_off = (int16_t)be16(r + 2);
			size_t off = data_off + be24(r + 5);

			memcpy(res->type, t, 4);
			res->type[4] = '\0';
			res->id = (int16_t)be16(r);
			res->attrs = r[4];
			res->reserved = be32(r + 8);

			if (off + 4 > len) {
				*why = "a resource points outside the file";
				goto fail;
			}
			res->length = be32(b + off);
			if (off + 4 + res->length > len) {
				*why = "a resource runs past the end of the file";
				goto fail;
			}
			res->data = (uint8_t *)(b + off + 4);

			if (name_off >= 0) {
				size_t at = name_list + (size_t)name_off;
				size_t nlen;

				if (at >= map_len) {
					*why = "a resource name lies outside "
					    "the map";
					goto fail;
				}
				nlen = map[at];
				if (at + 1 + nlen > map_len) {
					*why = "a resource name runs past the "
					    "end of the map";
					goto fail;
				}
				res->name = malloc(nlen + 1);
				if (res->name == NULL) {
					*why = "out of memory";
					goto fail;
				}
				memcpy(res->name, map + at + 1, nlen);
				res->name[nlen] = '\0';
			}
			slot++;
		}
	}

	out->count = slot;
	return (0);

fail:
	out->count = slot;
	resfork_free(out);
	return (-1);
}

void
resfork_free(struct resfork *rf)
{
	size_t i;

	for (i = 0; i < rf->count; i++)
		free(rf->items[i].name);
	free(rf->items);
	rf->items = NULL;
	rf->count = 0;
}

static void
wbe16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

static void
wbe32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

/*
 * The shape Apple's writers produce, which is the classic one: the data area
 * begins at 256 with the bytes before it left zero, the map follows it, and
 * the map's own first sixteen bytes are a copy of the file header.  The
 * caller supplies the file reference number; see resfork.h for why it is
 * copied rather than computed.
 */
#define RES_DATA_START	256
#define RES_MAP_HEADER	28

uint8_t *
resfork_build(const struct resource *items, size_t count, uint16_t fileref,
    size_t *out_len)
{
	size_t order[4096], ntypes = 0;
	size_t i, j, data_len = 0, names_len = 0, total_refs = 0;
	size_t map_off, type_list_len, name_list_off, map_len, total;
	uint8_t *b, *map;
	size_t at, ref_at, name_at;

	if (count > sizeof(order) / sizeof(order[0]))
		return (NULL);

	/* Types in first-seen order; order[] indexes the first of each. */
	for (i = 0; i < count; i++) {
		bool seen = false;

		for (j = 0; j < ntypes; j++)
			if (memcmp(items[order[j]].type, items[i].type, 4) == 0) {
				seen = true;
				break;
			}
		if (!seen)
			order[ntypes++] = i;

		data_len += 4 + items[i].length;
		if (items[i].name != NULL)
			names_len += 1 + strlen(items[i].name);
		total_refs++;
	}

	map_off = RES_DATA_START + data_len;
	type_list_len = 2 + ntypes * 8;
	name_list_off = RES_MAP_HEADER + type_list_len + total_refs * 12;
	map_len = name_list_off + names_len;
	total = map_off + map_len;

	b = calloc(1, total);
	if (b == NULL)
		return (NULL);

	wbe32(b, RES_DATA_START);
	wbe32(b + 4, (uint32_t)map_off);
	wbe32(b + 8, (uint32_t)data_len);
	wbe32(b + 12, (uint32_t)map_len);

	at = RES_DATA_START;
	for (i = 0; i < count; i++) {
		wbe32(b + at, items[i].length);
		if (items[i].length > 0)
			memcpy(b + at + 4, items[i].data, items[i].length);
		at += 4 + items[i].length;
	}

	map = b + map_off;
	memcpy(map, b, 16);
	/* A file with no resources carries no reference number either. */
	wbe16(map + 20, count == 0 ? 0 : fileref);
	wbe16(map + 24, RES_MAP_HEADER);
	wbe16(map + 26, (uint16_t)name_list_off);

	/* Counts are stored one less, so none of them is 0xffff. */
	wbe16(map + RES_MAP_HEADER, (uint16_t)(ntypes - 1));

	ref_at = RES_MAP_HEADER + type_list_len;
	name_at = name_list_off;

	for (j = 0; j < ntypes; j++) {
		uint8_t *t = map + RES_MAP_HEADER + 2 + j * 8;
		size_t nres = 0;

		memcpy(t, items[order[j]].type, 4);
		/* refOff is measured from the start of the type list. */
		wbe16(t + 6, (uint16_t)(ref_at - RES_MAP_HEADER));

		for (i = 0; i < count; i++) {
			uint8_t *r;
			size_t off;

			if (memcmp(items[i].type, items[order[j]].type, 4) != 0)
				continue;

			r = map + ref_at;
			wbe16(r, (uint16_t)items[i].id);

			if (items[i].name != NULL) {
				size_t nlen = strlen(items[i].name);

				wbe16(r + 2, (uint16_t)(name_at - name_list_off));
				map[name_at] = (uint8_t)nlen;
				memcpy(map + name_at + 1, items[i].name, nlen);
				name_at += 1 + nlen;
			} else {
				wbe16(r + 2, 0xffff);
			}

			r[4] = items[i].attrs;

			/* Three-byte offset into the data area. */
			off = 0;
			for (size_t k = 0; k < i; k++)
				off += 4 + items[k].length;
			r[5] = (uint8_t)(off >> 16);
			r[6] = (uint8_t)(off >> 8);
			r[7] = (uint8_t)off;

			wbe32(r + 8, items[i].reserved);

			ref_at += 12;
			nres++;
		}
		wbe16(t + 4, (uint16_t)(nres - 1));
	}

	*out_len = total;
	return (b);
}

int
resfork_write_file(const char *path, bool datafork, const uint8_t *bytes,
    size_t len)
{
	int fd;

	if (datafork) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0)
			return (-1);
		if (write(fd, bytes, len) != (ssize_t)len) {
			(void)close(fd);
			return (-1);
		}
		return (close(fd));
	}

	/* The file has to exist before it can be given a fork. */
	fd = open(path, O_WRONLY | O_CREAT, 0644);
	if (fd < 0)
		return (-1);
	(void)close(fd);

	return (setxattr(path, XATTR_RESOURCEFORK_NAME, bytes, len, 0, 0));
}
