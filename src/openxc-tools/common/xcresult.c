/*
 * xcresult.c -- see xcresult.h.
 *
 * The grammar, worked out from bundles Apple's tools produced:
 *
 *	value  := '[' type body ']'
 *	type   := 'T' value           -- a type object, spelled out
 *	        | 'S' len ':' name    -- the same type, referred to by name
 *	body   := member* | value*    -- members for an object, values for
 *	                                 an Array
 *	member := 'K' len ':' name (value | 'V' len ':' text)
 *
 * A type object carries _n, its name, and sometimes _s, the type it derives
 * from, which is itself a type object.  _v carries a scalar's text.  A type
 * is only spelled out at its first mention, so what it derives from has to
 * be remembered for the mentions after that.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "xcresult.h"

#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zstd.h>

#include "plist.h"

/* --- the object tree ---------------------------------------------- */

/* --- parsing ------------------------------------------------------ */

struct parser {
	const char	*p;
	const char	*end;
	bool		 failed;
	int		 bad_byte;	/* what stopped it, for the message */
};

static struct xcresult_node	*parse_value(struct parser *ps);

/*
 * Types are spelled out once and referred to by name after that, so what a
 * type derives from is only stated at its first mention.  This remembers it,
 * because Apple's output repeats the supertype at every mention.
 */
static struct {
	char		*name;
	struct xcresult_node	*supertype;
} seen_types[256];
static size_t	nseen_types;

static void
remember_type(const char *name, struct xcresult_node *supertype)
{
	size_t i;

	if (name == NULL || supertype == NULL)
		return;

	for (i = 0; i < nseen_types; i++)
		if (strcmp(seen_types[i].name, name) == 0)
			return;

	if (nseen_types < sizeof(seen_types) / sizeof(seen_types[0])) {
		seen_types[nseen_types].name = strdup(name);
		seen_types[nseen_types].supertype = supertype;
		nseen_types++;
	}
}

static struct xcresult_node *
recall_supertype(const char *name)
{
	size_t i;

	if (name == NULL)
		return (NULL);
	for (i = 0; i < nseen_types; i++)
		if (strcmp(seen_types[i].name, name) == 0)
			return (seen_types[i].supertype);
	return (NULL);
}

static bool
eat(struct parser *ps, char c)
{
	if (ps->p < ps->end && *ps->p == c) {
		ps->p++;
		return (true);
	}
	return (false);
}

/*
 * A counted token: a letter, a decimal length, a colon, then that many
 * bytes.  Returns them as a fresh string.
 */
static char *
parse_counted(struct parser *ps, char letter)
{
	size_t len = 0;
	char *out;

	if (ps->p >= ps->end || *ps->p != letter)
		return (NULL);
	ps->p++;

	while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9')
		len = len * 10 + (size_t)(*ps->p++ - '0');

	if (!eat(ps, ':') || (size_t)(ps->end - ps->p) < len) {
		ps->failed = true;
		return (NULL);
	}

	out = malloc(len + 1);
	if (out == NULL) {
		ps->failed = true;
		return (NULL);
	}
	memcpy(out, ps->p, len);
	out[len] = '\0';
	ps->p += len;
	return (out);
}

static void
add_member(struct xcresult_node *n, char *key, struct xcresult_node *value, char *text)
{
	struct xcresult_member *grown = realloc(n->members,
	    (n->nmembers + 1) * sizeof(*grown));

	if (grown == NULL)
		return;
	n->members = grown;
	n->members[n->nmembers].key = key;
	n->members[n->nmembers].value = value;
	n->members[n->nmembers].text = text;
	n->nmembers++;
}

static void
add_value(struct xcresult_node *n, struct xcresult_node *v)
{
	struct xcresult_node **grown = realloc(n->values,
	    (n->nvalues + 1) * sizeof(*grown));

	if (grown == NULL)
		return;
	n->values = grown;
	n->values[n->nvalues++] = v;
}

static struct xcresult_node *
parse_value(struct parser *ps)
{
	struct xcresult_node *n;

	if (!eat(ps, '[')) {
		ps->failed = true;
		if (ps->bad_byte < 0 && ps->p < ps->end) {
			const char *at = ps->p;

			/*
			 * Apple's reads a token first and only then finds it
			 * cannot use it, so a T, K, V or S at the very start
			 * is consumed and the byte after it is the one
			 * reported.  A test log beginning "Test Suite" is
			 * how that shows: theirs names the 'e', not the 'T'.
			 */
			if (*at == 'T' || *at == 'K' || *at == 'V' ||
			    *at == 'S')
				at++;
			ps->bad_byte = (at < ps->end) ?
			    (unsigned char)*at : -1;
		}
		return (NULL);
	}

	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		ps->failed = true;
		return (NULL);
	}

	/*
	 * The type, spelled out once and referred to by name afterwards.
	 * Only _n and _s are ever in it.
	 */
	if (ps->p < ps->end && *ps->p == 'T') {
		struct xcresult_node *t;

		ps->p++;
		t = parse_value(ps);
		if (t != NULL) {
			size_t i;

			for (i = 0; i < t->nmembers; i++) {
				if (strcmp(t->members[i].key, "_n") == 0)
					n->type = strdup(
					    t->members[i].text ?
					    t->members[i].text : "");
				else if (strcmp(t->members[i].key, "_s") == 0)
					/*
					 * _s is not a string but a type
					 * object like the one around it,
					 * and it can carry an _s of its
					 * own.
					 */
					n->supertype = t->members[i].value;
			}
		}
		remember_type(n->type, n->supertype);
	} else if (ps->p < ps->end && *ps->p == 'S') {
		n->type = parse_counted(ps, 'S');
		n->supertype = recall_supertype(n->type);
	}

	while (!ps->failed && ps->p < ps->end && *ps->p != ']') {
		if (*ps->p == 'K') {
			char *key = parse_counted(ps, 'K');

			if (key == NULL)
				break;
			if (ps->p < ps->end && *ps->p == 'V')
				add_member(n, key, NULL,
				    parse_counted(ps, 'V'));
			else
				add_member(n, key, parse_value(ps), NULL);
		} else if (*ps->p == '[') {
			add_value(n, parse_value(ps));	/* Array element */
		} else {
			ps->failed = true;
			ps->bad_byte = (unsigned char)*ps->p;
			break;
		}
	}

	(void)eat(ps, ']');
	return (n);
}


static char *
read_whole(const char *path, size_t *len)
{
	struct stat st;
	char *buf;
	FILE *fp = fopen(path, "rb");

	if (fp == NULL)
		return (NULL);
	if (fstat(fileno(fp), &st) != 0 || st.st_size < 0) {
		(void)fclose(fp);
		return (NULL);
	}
	buf = malloc((size_t)st.st_size + 1);
	if (buf == NULL || fread(buf, 1, (size_t)st.st_size, fp) !=
	    (size_t)st.st_size) {
		(void)fclose(fp);
		free(buf);
		return (NULL);
	}
	(void)fclose(fp);
	*len = (size_t)st.st_size;
	buf[*len] = '\0';
	return (buf);
}

char *
xcresult_root_id(const char *bundle)
{
	char path[PATH_MAX];
	plist_node *root, *id, *hash;
	char *text, *out = NULL;
	size_t len;

	(void)snprintf(path, sizeof(path), "%s/Info.plist", bundle);
	text = read_whole(path, &len);
	if (text == NULL)
		return (NULL);

	root = plist_parse_any(text, len);
	if (root != NULL &&
	    (id = plist_dict_get(root, "rootId")) != NULL &&
	    (hash = plist_dict_get(id, "hash")) != NULL &&
	    hash->string != NULL)
		out = strdup(hash->string);

	plist_free(root);
	free(text);
	return (out);
}

unsigned char *
xcresult_object_bytes(const char *bundle, const char *id, size_t *out_len)
{
	char path[PATH_MAX];
	char *raw, *plain;
	size_t len;

	(void)snprintf(path, sizeof(path), "%s/Data/data.%s", bundle, id);
	raw = read_whole(path, &len);
	if (raw == NULL)
		return (NULL);

	/*
	 * Small objects are stored as they are -- compressing forty bytes
	 * makes them bigger -- so anything without the zstd magic is
	 * already the serialisation.
	 */
	if (len < 4 || memcmp(raw, "\x28\xb5\x2f\xfd", 4) != 0) {
		*out_len = len;
		return ((unsigned char *)raw);
	}

	/*
	 * Streaming rather than one shot: these frames do not all declare
	 * their content size, and a frame that does not would otherwise be
	 * unreadable.
	 */
	{
		ZSTD_DStream *ds = ZSTD_createDStream();
		ZSTD_inBuffer in = { raw, len, 0 };
		size_t cap = len * 4 + 4096;
		size_t used = 0;

		plain = malloc(cap);
		if (ds == NULL || plain == NULL) {
			free(raw);
			free(plain);
			if (ds != NULL)
				(void)ZSTD_freeDStream(ds);
			return (NULL);
		}
		(void)ZSTD_initDStream(ds);

		for (;;) {
			ZSTD_outBuffer out = { plain, cap, used };
			size_t rc = ZSTD_decompressStream(ds, &out, &in);

			used = out.pos;
			if (ZSTD_isError(rc)) {
				free(raw);
				free(plain);
				(void)ZSTD_freeDStream(ds);
				return (NULL);
			}
			if (rc == 0 && in.pos == in.size)
				break;
			if (out.pos == cap) {
				char *grown = realloc(plain, cap * 2);

				if (grown == NULL) {
					free(raw);
					free(plain);
					(void)ZSTD_freeDStream(ds);
					return (NULL);
				}
				plain = grown;
				cap *= 2;
			} else if (in.pos == in.size && rc != 0) {
				break;		/* truncated frame */
			}
		}
		(void)ZSTD_freeDStream(ds);
		len = used;
	}
	*out_len = len;
	return ((unsigned char *)plain);
}


/* The two lines an error prints; --help prints the long form. */

struct xcresult_node *
xcresult_parse(const unsigned char *bytes, size_t len, int *bad_byte)
{
	struct parser ps;
	struct xcresult_node *n;

	ps.p = (const char *)bytes;
	ps.end = (const char *)bytes + len;
	ps.failed = false;
	ps.bad_byte = -1;

	n = parse_value(&ps);
	if (ps.failed) {
		*bad_byte = ps.bad_byte;
		return (NULL);
	}
	return (n);
}

struct xcresult_node *
xcresult_load(const char *bundle, const char *id, int *bad_byte)
{
	struct xcresult_node *n;
	unsigned char *bytes;
	size_t len;

	bytes = xcresult_object_bytes(bundle, id, &len);
	if (bytes == NULL)
		return (NULL);

	n = xcresult_parse(bytes, len, bad_byte);
	free(bytes);		/* the tree keeps copies */
	return (n);
}

const struct xcresult_node *
xcresult_get(const struct xcresult_node *n, const char *key)
{
	size_t i;

	if (n == NULL)
		return (NULL);
	for (i = 0; i < n->nmembers; i++)
		if (strcmp(n->members[i].key, key) == 0)
			return (n->members[i].value);
	return (NULL);
}

const char *
xcresult_get_text(const struct xcresult_node *n, const char *key)
{
	size_t i;

	if (n == NULL)
		return (NULL);
	for (i = 0; i < n->nmembers; i++)
		if (strcmp(n->members[i].key, key) == 0)
			return (n->members[i].text);
	return (NULL);
}

const char *
xcresult_reference_id(const struct xcresult_node *n)
{
	const struct xcresult_node *id = xcresult_get(n, "id");

	return (id == NULL ? NULL : xcresult_get_text(id, "_v"));
}
