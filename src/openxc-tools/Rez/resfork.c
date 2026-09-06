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
