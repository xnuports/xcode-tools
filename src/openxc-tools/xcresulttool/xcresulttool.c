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

#include "xcresult.h"

static const char *progname = "xcresulttool";

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

static void	print_node(const struct xcresult_node *n, int depth);

/* The name a type object carries, and the type it derives from. */
static const char *
type_name_of(const struct xcresult_node *t)
{
	size_t i;

	if (t == NULL)
		return (NULL);
	for (i = 0; i < t->nmembers; i++)
		if (strcmp(t->members[i].key, "_n") == 0)
			return (t->members[i].text);
	return (NULL);
}

static const struct xcresult_node *
type_super_of(const struct xcresult_node *t)
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
print_supertype(const struct xcresult_node *t, int depth)
{
	const struct xcresult_node *super = type_super_of(t);

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
print_type(const struct xcresult_node *n, int depth)
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
print_node(const struct xcresult_node *n, int depth)
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
	struct xcresult_node *n;
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
		root_id = xcresult_root_id(bundle);
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
	n = xcresult_load(bundle, id, &bad_byte);
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
