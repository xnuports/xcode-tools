/*
 * xcresulttool -- read a .xcresult bundle.
 *
 *	xcresulttool get object --path <bundle> [--id <id>] [--format json]
 *
 * A result bundle is a content-addressed store.  Info.plist names a root
 * object and the storage backend; each object lives in Data/data.<id>,
 * zstd-compressed, and holds a small self-describing serialisation:
 *
 *	value  := '[' type body ']'
 *	type   := 'T' value           -- a type object, spelled out
 *	        | 'S' len ':' name    -- the same type, referred to by name
 *	body   := member* | value*    -- members for an object, values for
 *	                                 an Array
 *	member := 'K' len ':' name (value | 'V' len ':' text)
 *
 * A type object is just an object carrying _n, the type's name, and
 * sometimes _s for the type it derives from.  _v carries a scalar's text.
 * References between objects are ordinary objects of type Reference whose
 * id names another file in Data.
 *
 * The format was worked out from bundles this machine produced; Apple
 * document none of it.  The JSON printed here is what their own
 * "get object --format json --legacy" prints, which is what the comparison
 * checks.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zstd.h>

#include "plist.h"

static const char *progname = "xcresulttool";

/* --- the object tree ---------------------------------------------- */

struct node;

struct member {
	char		*key;
	struct node	*value;		/* NULL when the member is text */
	char		*text;		/* set for K...V... members */
};

struct node {
	char		*type;		/* _n of the type, or NULL */
	struct node	*supertype;	/* _s: a type object of its own */
	struct member	*members;
	size_t		nmembers;
	struct node	**values;	/* Array elements */
	size_t		nvalues;
};

/* --- parsing ------------------------------------------------------ */

struct parser {
	const char	*p;
	const char	*end;
	bool		 failed;
	int		 bad_byte;	/* what stopped it, for the message */
};

static struct node	*parse_value(struct parser *ps);

/*
 * Types are spelled out once and referred to by name after that, so what a
 * type derives from is only stated at its first mention.  This remembers it,
 * because Apple's output repeats the supertype at every mention.
 */
static struct {
	char		*name;
	struct node	*supertype;
} seen_types[256];
static size_t	nseen_types;

static void
remember_type(const char *name, struct node *supertype)
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

static struct node *
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
add_member(struct node *n, char *key, struct node *value, char *text)
{
	struct member *grown = realloc(n->members,
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
add_value(struct node *n, struct node *v)
{
	struct node **grown = realloc(n->values,
	    (n->nvalues + 1) * sizeof(*grown));

	if (grown == NULL)
		return;
	n->values = grown;
	n->values[n->nvalues++] = v;
}

static struct node *
parse_value(struct parser *ps)
{
	struct node *n;

	if (!eat(ps, '[')) {
		ps->failed = true;
		if (ps->bad_byte < 0 && ps->p < ps->end)
			ps->bad_byte = (unsigned char)*ps->p;
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
		struct node *t;

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

/* --- JSON ---------------------------------------------------------- */

static void
indent(int depth)
{
	int i;

	for (i = 0; i < depth; i++)
		(void)fputs("  ", stdout);
}

static void
print_json_string(const char *s)
{
	(void)putchar('"');
	for (; *s != '\0'; s++) {
		unsigned char c = (unsigned char)*s;

		switch (c) {
		case '"':  (void)fputs("\\\"", stdout); break;
		case '\\': (void)fputs("\\\\", stdout); break;
		case '\n': (void)fputs("\\n", stdout); break;
		case '\r': (void)fputs("\\r", stdout); break;
		case '\t': (void)fputs("\\t", stdout); break;
		/* Apple's escapes the solidus, as JSONSerialization does. */
		case '/':  (void)fputs("\\/", stdout); break;
		default:
			if (c < 0x20)
				(void)printf("\\u%04x", c);
			else
				(void)putchar((char)c);
		}
	}
	(void)putchar('"');
}

static void	print_node(const struct node *n, int depth);

/* The name a type object carries, and the type it derives from. */
static const char *
type_name_of(const struct node *t)
{
	size_t i;

	if (t == NULL)
		return (NULL);
	for (i = 0; i < t->nmembers; i++)
		if (strcmp(t->members[i].key, "_n") == 0)
			return (t->members[i].text);
	return (NULL);
}

static const struct node *
type_super_of(const struct node *t)
{
	size_t i;

	if (t == NULL)
		return (NULL);
	for (i = 0; i < t->nmembers; i++)
		if (strcmp(t->members[i].key, "_s") == 0)
			return (t->members[i].value);
	return (NULL);
}

static void
print_supertype(const struct node *t, int depth)
{
	const struct node *super = type_super_of(t);

	(void)fputs("{\n", stdout);
	indent(depth + 1);
	(void)fputs("\"_name\" : ", stdout);
	print_json_string(type_name_of(t) != NULL ? type_name_of(t) : "");

	if (super != NULL) {
		(void)fputs(",\n", stdout);
		indent(depth + 1);
		(void)fputs("\"_supertype\" : ", stdout);
		print_supertype(super, depth + 1);
	}
	(void)fputc('\n', stdout);
	indent(depth);
	(void)fputc('}', stdout);
}

static void
print_type(const struct node *n, int depth)
{
	indent(depth);
	(void)fputs("\"_type\" : {\n", stdout);
	indent(depth + 1);
	(void)fputs("\"_name\" : ", stdout);
	print_json_string(n->type != NULL ? n->type : "");

	if (n->supertype != NULL) {
		(void)fputs(",\n", stdout);
		indent(depth + 1);
		(void)fputs("\"_supertype\" : ", stdout);
		print_supertype(n->supertype, depth + 1);
	}
	(void)fputc('\n', stdout);
	indent(depth);
	(void)fputc('}', stdout);
}

static void
print_node(const struct node *n, int depth)
{
	bool first = true;
	size_t i;

	(void)fputs("{\n", stdout);

	if (n->type != NULL) {
		print_type(n, depth + 1);
		first = false;
	}

	for (i = 0; i < n->nmembers; i++) {
		if (!first)
			(void)fputs(",\n", stdout);
		first = false;

		indent(depth + 1);
		/* _v is spelled _value in the JSON, as _n is _name. */
		print_json_string(strcmp(n->members[i].key, "_v") == 0 ?
		    "_value" : n->members[i].key);
		(void)fputs(" : ", stdout);

		if (n->members[i].text != NULL)
			print_json_string(n->members[i].text);
		else if (n->members[i].value != NULL)
			print_node(n->members[i].value, depth + 1);
		else
			(void)fputs("null", stdout);
	}

	if (n->nvalues > 0 || (n->type != NULL &&
	    strcmp(n->type, "Array") == 0)) {
		if (!first)
			(void)fputs(",\n", stdout);
		first = false;

		indent(depth + 1);
		(void)fputs("\"_values\" : [\n", stdout);
		for (i = 0; i < n->nvalues; i++) {
			indent(depth + 2);
			print_node(n->values[i], depth + 2);
			if (i + 1 < n->nvalues)
				(void)fputc(',', stdout);
			(void)fputc('\n', stdout);
		}
		indent(depth + 1);
		(void)fputc(']', stdout);
	}

	(void)fputc('\n', stdout);
	indent(depth);
	(void)fputc('}', stdout);
}

/* --- the bundle ---------------------------------------------------- */

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

/* The id of the root object, out of the bundle's Info.plist. */
static char *
bundle_root_id(const char *bundle)
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

static struct node *
load_object(const char *bundle, const char *id, int *bad_byte)
{
	char path[PATH_MAX];
	struct parser ps;
	struct node *n;
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
		plain = raw;
		goto parse;
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
parse:
	plain[len] = '\0';

	ps.p = plain;
	ps.end = plain + len;
	ps.failed = false;
	ps.bad_byte = -1;
	n = parse_value(&ps);
	if (ps.failed) {
		*bad_byte = ps.bad_byte;
		free(plain);
		return (NULL);
	}
	/* The tree keeps copies; the buffer is not needed past here. */
	free(plain);
	return (n);
}

/* The two lines an error prints; --help prints the long form. */
static void
usage_short(void)
{
	(void)fprintf(stderr, "Usage: xcresulttool <subcommand>\n"
	    "  See 'xcresulttool --help' for more information.\n");
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "OVERVIEW: Xcode Result Bundle Tool\n\n"
	    "USAGE: xcresulttool <subcommand>\n\n"
	    "OPTIONS:\n"
	    "  --version               Show the version.\n"
	    "  -h, --help              Show help information.\n\n"
	    "SUBCOMMANDS:\n"
	    "  get                     Get Result Bundle contents.\n");
}

int
main(int argc, char *argv[])
{
	const char *bundle = NULL, *id = NULL;
	struct node *n;
	char *root_id = NULL;
	int bad_byte, i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "--path") == 0 && i + 1 < argc)
			bundle = argv[++i];
		else if (strcmp(a, "--id") == 0 && i + 1 < argc)
			id = argv[++i];
		else if (strcmp(a, "--format") == 0 && i + 1 < argc)
			i++;			/* json is all this prints */
		else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
			usage();
			return (0);
		} else if (a[0] == '-') {
			/* --legacy and the rest are accepted and ignored */
			continue;
		}
	}

	if (bundle == NULL) {
		(void)fprintf(stderr, "Error: The required option "
		    "'--path' is missing.\n");
		return (64);
	}

	{
		struct stat st;

		if (stat(bundle, &st) != 0) {
			(void)fprintf(stderr, "Error: File or directory "
			    "doesn't exist at path: %s.\n", bundle);
			usage_short();
			return (64);
		}
	}

	if (id == NULL) {
		root_id = bundle_root_id(bundle);
		if (root_id == NULL) {
			(void)fprintf(stderr, "Error: %s is not a result "
			    "bundle.\n", bundle);
			return (1);
		}
		id = root_id;
	}

	{
		char path[PATH_MAX];
		struct stat st;

		(void)snprintf(path, sizeof(path), "%s/Data/data.%s", bundle,
		    id);
		if (stat(path, &st) != 0) {
			(void)fprintf(stderr, "Error: cannot create DataID "
			    "with hash %s\n", id);
			free(root_id);
			return (1);
		}
	}

	bad_byte = -1;
	n = load_object(bundle, id, &bad_byte);
	if (n == NULL) {
		/*
		 * Not every object in the store is one of these: logs and
		 * attachments sit there as themselves.  Apple's names the
		 * byte that was not a token, which for a JSON log is '{'.
		 */
		(void)fprintf(stderr, "Error: failed to parse bytes as tokens, "
		    "is this really a structured object? Underlying error: "
		    "unrecognizedToken(%d)\n", bad_byte);
		free(root_id);
		return (1);
	}

	print_node(n, 0);
	(void)fputc('\n', stdout);

	free(root_id);
	(void)progname;
	return (0);
}
