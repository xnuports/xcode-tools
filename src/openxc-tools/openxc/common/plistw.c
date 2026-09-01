/*
 * plistw.c -- build a property list and write it out.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "plistw.h"

enum pw_type {
	PW_DICT,
	PW_STRING
};

struct pw_member {
	char *key;
	pw_node *value;
};

struct pw_node {
	enum pw_type type;
	char *string;			/* PW_STRING */
	struct pw_member *members;	/* PW_DICT */
	size_t count;
	size_t cap;
};

/* ------------------------------------------------------------------ */
/* construction                                                         */
/* ------------------------------------------------------------------ */

pw_node *
pw_dict(void)
{
	pw_node *n = calloc(1, sizeof(*n));

	if (n != NULL)
		n->type = PW_DICT;
	return n;
}

pw_node *
pw_string(const char *value)
{
	pw_node *n = calloc(1, sizeof(*n));

	if (n == NULL)
		return NULL;

	n->type = PW_STRING;
	if ((n->string = strdup((value != NULL) ? value : "")) == NULL) {
		free(n);
		return NULL;
	}
	return n;
}

int
pw_dict_set(pw_node *dict, const char *key, pw_node *value)
{
	char *k;

	if (dict == NULL || dict->type != PW_DICT || key == NULL || value == NULL)
		return -1;

	if (dict->count == dict->cap) {
		size_t cap = (dict->cap == 0) ? 8 : dict->cap * 2;
		struct pw_member *m = realloc(dict->members, cap * sizeof(*m));

		if (m == NULL)
			return -1;
		dict->members = m;
		dict->cap = cap;
	}

	if ((k = strdup(key)) == NULL)
		return -1;

	dict->members[dict->count].key = k;
	dict->members[dict->count].value = value;
	dict->count++;
	return 0;
}

void
pw_free(pw_node *node)
{
	size_t i;

	if (node == NULL)
		return;

	for (i = 0; i < node->count; i++) {
		free(node->members[i].key);
		pw_free(node->members[i].value);
	}

	free(node->members);
	free(node->string);
	free(node);
}

static int
cmp_member(const void *a, const void *b)
{
	return strcmp(((const struct pw_member *)a)->key,
	    ((const struct pw_member *)b)->key);
}

/* Sorted view of a dict's members; caller frees. */
static struct pw_member *
sorted_members(const pw_node *dict)
{
	struct pw_member *m;

	if (dict->count == 0)
		return NULL;

	if ((m = malloc(dict->count * sizeof(*m))) == NULL)
		return NULL;

	memcpy(m, dict->members, dict->count * sizeof(*m));
	qsort(m, dict->count, sizeof(*m), cmp_member);
	return m;
}

/* ------------------------------------------------------------------ */
/* XML                                                                  */
/* ------------------------------------------------------------------ */

static void
xml_escape(FILE *fp, const char *s)
{
	for (; *s != '\0'; s++) {
		switch (*s) {
		case '&': fputs("&amp;", fp); break;
		case '<': fputs("&lt;", fp); break;
		case '>': fputs("&gt;", fp); break;
		default:  fputc(*s, fp); break;
		}
	}
}

static void
indent(FILE *fp, int depth)
{
	int i;

	for (i = 0; i < depth; i++)
		fputc('\t', fp);
}

static void
xml_node(FILE *fp, const pw_node *node, int depth)
{
	struct pw_member *m;
	size_t i;

	if (node->type == PW_STRING) {
		fputs("<string>", fp);
		xml_escape(fp, node->string);
		fputs("</string>\n", fp);
		return;
	}

	fputs("<dict>\n", fp);
	m = sorted_members(node);
	for (i = 0; i < node->count; i++) {
		indent(fp, depth + 1);
		fputs("<key>", fp);
		xml_escape(fp, m[i].key);
		fputs("</key>\n", fp);
		indent(fp, depth + 1);
		xml_node(fp, m[i].value, depth + 1);
	}
	free(m);
	indent(fp, depth);
	fputs("</dict>\n", fp);
}

int
pw_write_xml(FILE *fp, const pw_node *root)
{
	if (fp == NULL || root == NULL)
		return -1;

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\""
	      " \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
	      "<plist version=\"1.0\">\n", fp);
	xml_node(fp, root, 0);
	fputs("</plist>\n", fp);

	return 0;
}

/* ------------------------------------------------------------------ */
/* binary                                                               */
/* ------------------------------------------------------------------ */

/*
 * The object table is built before anything is written, because every
 * reference is an index into it and the trailer needs its size.
 *
 * Strings are pooled: the same text appearing as several keys or values
 * becomes one object, which is what CoreFoundation does and what keeps a
 * catalog's repeated values from being written once per use.
 */
struct bp_obj {
	const pw_node *node;	/* dicts */
	const char *string;	/* strings */
};

struct bp {
	struct bp_obj *objs;
	size_t count;
	size_t cap;
};

static long
bp_add(struct bp *bp, const pw_node *node, const char *string)
{
	size_t i;

	if (string != NULL)
		for (i = 0; i < bp->count; i++)
			if (bp->objs[i].string != NULL &&
			    strcmp(bp->objs[i].string, string) == 0)
				return (long)i;

	if (bp->count == bp->cap) {
		size_t cap = (bp->cap == 0) ? 16 : bp->cap * 2;
		struct bp_obj *o = realloc(bp->objs, cap * sizeof(*o));

		if (o == NULL)
			return -1;
		bp->objs = o;
		bp->cap = cap;
	}

	bp->objs[bp->count].node = node;
	bp->objs[bp->count].string = string;
	return (long)bp->count++;
}

/*
 * Number the objects.  A dict is numbered before its contents, and its
 * keys before its values, which is the order CoreFoundation writes and
 * keeps the reference lists contiguous.
 */
static int
bp_collect(struct bp *bp, const pw_node *node)
{
	struct pw_member *m;
	size_t i;

	if (node->type == PW_STRING)
		return (bp_add(bp, NULL, node->string) < 0) ? -1 : 0;

	if (bp_add(bp, node, NULL) < 0)
		return -1;

	if ((m = sorted_members(node)) == NULL && node->count > 0)
		return -1;

	for (i = 0; i < node->count; i++)
		if (bp_add(bp, NULL, m[i].key) < 0) {
			free(m);
			return -1;
		}

	for (i = 0; i < node->count; i++)
		if (bp_collect(bp, m[i].value) != 0) {
			free(m);
			return -1;
		}

	free(m);
	return 0;
}

static long
bp_index_of_string(const struct bp *bp, const char *s)
{
	size_t i;

	for (i = 0; i < bp->count; i++)
		if (bp->objs[i].string != NULL &&
		    strcmp(bp->objs[i].string, s) == 0)
			return (long)i;

	return -1;
}

static long
bp_index_of_node(const struct bp *bp, const pw_node *n)
{
	size_t i;

	if (n->type == PW_STRING)
		return bp_index_of_string(bp, n->string);

	for (i = 0; i < bp->count; i++)
		if (bp->objs[i].node == n)
			return (long)i;

	return -1;
}

static void
put_be(FILE *fp, uint64_t v, int width)
{
	int i;

	for (i = width - 1; i >= 0; i--)
		fputc((int)((v >> (i * 8)) & 0xFF), fp);
}

static int
byte_width(uint64_t v)
{
	if (v <= 0xFF)
		return 1;
	if (v <= 0xFFFF)
		return 2;
	if (v <= 0xFFFFFFFFULL)
		return 4;
	return 8;
}

/* A marker whose low nibble is the count, or 0xF plus an int object. */
static void
put_marker(FILE *fp, unsigned base, size_t count)
{
	if (count < 15) {
		fputc((int)(base | count), fp);
		return;
	}

	fputc((int)(base | 0x0F), fp);

	if (count <= 0xFF) {
		fputc(0x10, fp);
		put_be(fp, count, 1);
	} else if (count <= 0xFFFF) {
		fputc(0x11, fp);
		put_be(fp, count, 2);
	} else {
		fputc(0x12, fp);
		put_be(fp, count, 4);
	}
}

static int
is_ascii(const char *s)
{
	for (; *s != '\0'; s++)
		if ((unsigned char)*s >= 0x80)
			return 0;

	return 1;
}

/*
 * UTF-8 to UTF-16BE.  Returns the number of code units written, or -1;
 * pass a NULL fp to count without writing.
 */
static long
put_utf16(FILE *fp, const char *s)
{
	const unsigned char *p = (const unsigned char *)s;
	long units = 0;

	while (*p != '\0') {
		unsigned long cp;
		int extra;

		if (*p < 0x80) {
			cp = *p++;
			extra = 0;
		} else if ((*p & 0xE0) == 0xC0) {
			cp = *p++ & 0x1FU;
			extra = 1;
		} else if ((*p & 0xF0) == 0xE0) {
			cp = *p++ & 0x0FU;
			extra = 2;
		} else if ((*p & 0xF8) == 0xF0) {
			cp = *p++ & 0x07U;
			extra = 3;
		} else {
			return -1;
		}

		while (extra-- > 0) {
			if ((*p & 0xC0) != 0x80)
				return -1;
			cp = (cp << 6) | (unsigned long)(*p++ & 0x3F);
		}

		if (cp >= 0x10000) {
			unsigned long v = cp - 0x10000;

			if (fp != NULL) {
				put_be(fp, 0xD800 | (v >> 10), 2);
				put_be(fp, 0xDC00 | (v & 0x3FF), 2);
			}
			units += 2;
		} else {
			if (fp != NULL)
				put_be(fp, cp, 2);
			units++;
		}
	}

	return units;
}

static int
write_object(FILE *fp, const struct bp *bp, const struct bp_obj *obj,
    int ref_size)
{
	struct pw_member *m;
	size_t i;

	if (obj->string != NULL) {
		if (is_ascii(obj->string)) {
			size_t n = strlen(obj->string);

			put_marker(fp, 0x50, n);
			fwrite(obj->string, 1, n, fp);
		} else {
			long units = put_utf16(NULL, obj->string);

			if (units < 0)
				return -1;
			put_marker(fp, 0x60, (size_t)units);
			if (put_utf16(fp, obj->string) < 0)
				return -1;
		}
		return 0;
	}

	put_marker(fp, 0xD0, obj->node->count);

	if ((m = sorted_members(obj->node)) == NULL && obj->node->count > 0)
		return -1;

	for (i = 0; i < obj->node->count; i++) {
		long idx = bp_index_of_string(bp, m[i].key);

		if (idx < 0) {
			free(m);
			return -1;
		}
		put_be(fp, (uint64_t)idx, ref_size);
	}

	for (i = 0; i < obj->node->count; i++) {
		long idx = bp_index_of_node(bp, m[i].value);

		if (idx < 0) {
			free(m);
			return -1;
		}
		put_be(fp, (uint64_t)idx, ref_size);
	}

	free(m);
	return 0;
}

int
pw_write_binary(FILE *fp, const pw_node *root)
{
	struct bp bp;
	uint64_t *offsets = NULL;
	long table_off;
	int ref_size, off_size, rc = -1;
	size_t i;

	if (fp == NULL || root == NULL)
		return -1;

	memset(&bp, 0, sizeof(bp));
	if (bp_collect(&bp, root) != 0)
		goto out;

	if ((offsets = calloc(bp.count, sizeof(*offsets))) == NULL)
		goto out;

	ref_size = byte_width(bp.count);

	fwrite("bplist00", 1, 8, fp);

	for (i = 0; i < bp.count; i++) {
		long here = ftell(fp);

		if (here < 0)
			goto out;
		offsets[i] = (uint64_t)here;

		if (write_object(fp, &bp, &bp.objs[i], ref_size) != 0)
			goto out;
	}

	if ((table_off = ftell(fp)) < 0)
		goto out;

	off_size = byte_width((uint64_t)table_off);
	for (i = 0; i < bp.count; i++)
		put_be(fp, offsets[i], off_size);

	/* Trailer: five unused bytes, the sort version, then the sizes. */
	for (i = 0; i < 6; i++)
		fputc(0, fp);
	fputc(off_size, fp);
	fputc(ref_size, fp);
	put_be(fp, bp.count, 8);
	put_be(fp, 0, 8);			/* top object */
	put_be(fp, (uint64_t)table_off, 8);

	rc = 0;
out:
	free(offsets);
	free(bp.objs);
	return rc;
}
