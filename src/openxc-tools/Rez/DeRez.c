/*
 * DeRez -- decompile a resource fork back into Rez source.
 *
 *	DeRez resourceFile [-only type[(id[:id])]] [-s type[(id[:id])]]
 *	      [-useDF] [-p]
 *
 * With no type declarations to go on, every resource comes out as a data
 * statement: the bytes in hex with their printable form in a comment
 * alongside.  That is what Apple's does when it is given no .r files, and
 * it is all that can be done without the Rez language to say what the
 * bytes mean -- type-directed output waits on the compiler.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resfork.h"

/* Bytes per hex line, and the column the comment starts in. */
#define BYTES_PER_LINE	16
#define COMMENT_COLUMN	56

static const char *progname = "DeRez";

/* One -only or -s selector: a type, optionally narrowed to an id range. */
struct filter {
	char	type[5];
	bool	quoted;
	bool	has_range;
	long	first, last;
};

static struct filter	includes[64], excludes[64];
static size_t		nincludes, nexcludes;

static void
fail(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "### %s - ", progname);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fputc('\n', stderr);
}

static void
usage(void)
{
	fail("Usage: %s resourceFile [-c] [-d name[=value]] [-e] [-i path] "
	    "[-isysroot path] [-m n] [-noResolve [output | include]] "
	    "[-only type[(id[:id])]] [-p] [-rd] [-s type[(id[:id])]] "
	    "[-sc script] [-u name] [-useDF] [file...].", progname);
}

/*
 * A selector is 'TYPE' or TYPE, optionally followed by (id) or (id:id).
 * Quotes are what a shell usually eats, so both spellings are taken.
 */
static bool
parse_filter(const char *arg, struct filter *f)
{
	const char *p = arg;
	size_t n = 0;

	f->quoted = (*p == '\'');
	if (f->quoted)
		p++;
	while (*p != '\0' && *p != '\'' && *p != '(' && n < 4)
		f->type[n++] = *p++;
	while (n < 4)
		f->type[n++] = ' ';
	f->type[4] = '\0';

	if (*p == '\'')
		p++;

	/* "'STR ' (0)" is as good as "'STR '(0)". */
	while (*p == ' ' || *p == '\t')
		p++;

	f->has_range = false;
	if (*p == '(' && !f->quoted) {
		/*
		 * Unquoted, Apple's parser reads the type as a variable
		 * rather than a literal, and says so before giving up.  The
		 * name it reports is what was written, not the padded type.
		 */
		size_t n2 = (size_t)(p - arg);

		while (n2 > 0 && (arg[n2 - 1] == ' ' || arg[n2 - 1] == '\t'))
			n2--;
		fail("Undefined variable (%.*s) not allowed in expression.",
		    (int)n2, arg);
		return (false);
	}
	if (*p == '(') {
		char *end;

		p++;
		f->first = strtol(p, &end, 0);
		if (end == p)
			return (false);
		p = end;
		f->last = f->first;
		if (*p == ':') {
			p++;
			f->last = strtol(p, &end, 0);
			if (end == p)
				return (false);
			p = end;
		}
		if (*p != ')')
			return (false);
		f->has_range = true;
	}
	return (true);
}

static bool
filter_matches(const struct filter *f, const struct resource *r)
{
	if (memcmp(f->type, r->type, 4) != 0)
		return (false);
	if (!f->has_range)
		return (true);
	return (r->id >= f->first && r->id <= f->last);
}

static bool
wanted(const struct resource *r)
{
	size_t i;

	for (i = 0; i < nexcludes; i++)
		if (filter_matches(&excludes[i], r))
			return (false);

	if (nincludes == 0)
		return (true);

	for (i = 0; i < nincludes; i++)
		if (filter_matches(&includes[i], r))
			return (true);

	return (false);
}

/* A string as Rez writes one, with the quote and backslash escaped. */
static void
print_quoted(const char *s)
{
	(void)putchar('"');
	for (; *s != '\0'; s++) {
		if (*s == '"' || *s == '\\')
			(void)putchar('\\');
		(void)putchar(*s);
	}
	(void)putchar('"');
}

static void
print_attributes(uint8_t attrs)
{
	static const struct {
		uint8_t		bit;
		const char	*name;
	} names[] = {
		{ RES_SYSHEAP,	 "sysheap" },
		{ RES_PURGEABLE, "purgeable" },
		{ RES_LOCKED,	 "locked" },
		{ RES_PROTECTED, "protected" },
		{ RES_PRELOAD,	 "preload" },
	};
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if ((attrs & names[i].bit) != 0)
			(void)printf(", %s", names[i].name);
}

/*
 * Sixteen bytes to a line, in two-byte groups, then the same bytes as
 * characters.  Anything below a space and DEL become dots; everything
 * above is written through, high bytes included.
 */
static void
print_data(const uint8_t *data, uint32_t length)
{
	uint32_t at;

	for (at = 0; at < length; at += BYTES_PER_LINE) {
		uint32_t n = length - at;
		uint32_t i;
		int width = 0;

		if (n > BYTES_PER_LINE)
			n = BYTES_PER_LINE;

		(void)putchar('\t');
		width += 1;
		width += printf("$\"");
		for (i = 0; i < n; i++) {
			if (i > 0 && i % 2 == 0)
				width += printf(" ");
			width += printf("%02X", data[at + i]);
		}
		width += printf("\"");

		while (width < COMMENT_COLUMN - 1) {
			(void)putchar(' ');
			width++;
		}

		(void)printf("/* ");
		for (i = 0; i < n; i++) {
			uint8_t c = data[at + i];

			(void)putchar((c < 0x20 || c == 0x7f) ? '.' : c);
		}
		(void)printf(" */\n");
	}
}

static int
derez(const char *path, bool datafork)
{
	struct resfork rf;
	const char *why = "";
	uint8_t *bytes;
	size_t len, i;
	bool first = true;

	{
		struct stat st;

		/* A directory has no fork to miss, and is reported as that
		 * rather than as an empty file. */
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			fail("errFSForkNotFound (-1409) during open of "
			    "resource file \"%s\".", path);
			return (2);
		}
	}

	bytes = resfork_read_file(path, datafork, &len);
	if (bytes == NULL) {
		fail("errFSBadFSRef (-1401) during open of resource file "
		    "\"%s\".", path);
		return (2);
	}

	/*
	 * A file with nothing in its fork is not a resource file with no
	 * resources; Apple's reads it as running out of file.
	 */
	if (len == 0) {
		fail("eofErr (-39) during open of resource file \"%s\".",
		    path);
		free(bytes);
		return (2);
	}

	if (resfork_parse(bytes, len, &rf, &why) != 0) {
		fail("Fatal Error: %s.", why);
		free(bytes);
		return (1);
	}

	for (i = 0; i < rf.count; i++) {
		const struct resource *r = &rf.items[i];

		if (!wanted(r))
			continue;

		if (!first)
			(void)putchar('\n');
		first = false;

		(void)printf("data '%s' (%d", r->type, r->id);
		if (r->name != NULL) {
			(void)printf(", ");
			print_quoted(r->name);
		}
		print_attributes(r->attrs);
		(void)printf(") {\n");

		print_data(r->data, r->length);

		(void)printf("};\n");
	}

	resfork_free(&rf);
	free(bytes);
	return (0);
}

int
main(int argc, char *argv[])
{
	const char *path = NULL;
	bool datafork = false;
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (a[0] != '-') {
			if (path == NULL)
				path = a;
			continue;
		}

		if (strcmp(a, "-useDF") == 0) {
			datafork = true;
		} else if (strcmp(a, "-only") == 0 || strcmp(a, "-s") == 0) {
			bool only = (strcmp(a, "-only") == 0);
			struct filter *f;

			if (++i >= argc) {
				usage();
				return (1);
			}
			f = only ? &includes[nincludes] : &excludes[nexcludes];
			if (!parse_filter(argv[i], f)) {
				fail("Bad argument \"%s\" to %s option.",
				    argv[i], a);
				usage();
				return (1);
			}
			if (only)
				nincludes++;
			else
				nexcludes++;
		} else if (strcmp(a, "-p") == 0 || strcmp(a, "-c") == 0 ||
		    strcmp(a, "-e") == 0 || strcmp(a, "-rd") == 0) {
			/* Accepted and without effect here: each of these
			 * shapes type-directed output, which needs the type
			 * declarations this does not read. */
		} else if (strcmp(a, "-d") == 0 || strcmp(a, "-i") == 0 ||
		    strcmp(a, "-isysroot") == 0 || strcmp(a, "-m") == 0 ||
		    strcmp(a, "-sc") == 0 || strcmp(a, "-u") == 0 ||
		    strcmp(a, "-noResolve") == 0) {
			i++;			/* takes an argument */
		} else {
			fail("The option %s is not yet implemented.", a);
			usage();
			return (1);
		}
	}

	if (nincludes > 0 && nexcludes > 0) {
		fail("The -s option conflicts with the -only option.");
		usage();
		return (1);
	}

	if (path == NULL) {
		fail("No filename to de-compile was specified.");
		usage();
		return (1);
	}

	return (derez(path, datafork));
}
