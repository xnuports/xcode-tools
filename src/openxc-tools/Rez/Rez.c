/*
 * Rez -- compile resource descriptions into a resource fork.
 *
 *	Rez [-a] [-c creator] [-o file] [-t type] [-useDF] [file...]
 *
 * The language has six top-level statements.  This compiles the two that
 * describe resources literally -- data, which carries the bytes, and read,
 * which takes them from a file -- and reports the other four rather than
 * guessing at them.  type and resource together are a small type system,
 * where a resource is laid out according to a declaration given earlier;
 * that is the rest of the work, and quietly mis-compiling it would be worse
 * than saying so.
 *
 * What this does cover is the round trip: DeRez writes data statements, and
 * these read them back into the same fork.
 *
 * One known divergence, in a corner: with three or more successive -a runs,
 * where an earlier one replaced a resource, the bytes in the data area come
 * out in a different order than Apple's, though the map and every resource
 * are the same.  A single -a matches across every combination tried.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "finderinfo.h"
#include "resfork.h"

#define MAX_RESOURCES	4096

static const char	*progname = "Rez";
static const char	*srcname = "";
static int		 line = 1;
static int		 errors;

/* The source being read, and where we are in it. */
static const char	*src;
static size_t		 at, srclen;

static void
diag(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s:%d: ### %s - ", srcname, line, progname);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fputc('\n', stderr);
	errors++;
}

static void
usage(void)
{
	(void)fprintf(stderr, "### %s - Usage: Rez [-a] [-al word | longword] "
	    "[-arch arch] [-c creator] [-d name[=value]] [-i path] "
	    "[-isysroot path] [-F framework] [-m] [-noResolve [output | "
	    "include]] [-o file] [-ov] [-p] [-rd] [-ro] [-s path] "
	    "[-sc script] [-t type] [-u name] [-useDF] [file...].\n",
	    progname);
}

/* --- lexing ------------------------------------------------------- */

static void
skip_space(void)
{
	while (at < srclen) {
		if (src[at] == '\n') {
			line++;
			at++;
		} else if (isspace((unsigned char)src[at])) {
			at++;
		} else if (src[at] == '/' && at + 1 < srclen &&
		    src[at + 1] == '/') {
			while (at < srclen && src[at] != '\n')
				at++;
		} else if (src[at] == '/' && at + 1 < srclen &&
		    src[at + 1] == '*') {
			at += 2;
			while (at + 1 < srclen &&
			    !(src[at] == '*' && src[at + 1] == '/')) {
				if (src[at] == '\n')
					line++;
				at++;
			}
			at = (at + 2 > srclen) ? srclen : at + 2;
		} else {
			return;
		}
	}
}

static bool
peek_char(char c)
{
	skip_space();
	return (at < srclen && src[at] == c);
}

static bool
take_char(char c)
{
	if (!peek_char(c))
		return (false);
	at++;
	return (true);
}

/* An identifier, lowercased into buf for keyword comparison. */
static bool
take_word(char *buf, size_t buflen)
{
	size_t n = 0;

	skip_space();
	if (at >= srclen || !(isalpha((unsigned char)src[at]) || src[at] == '_'))
		return (false);

	while (at < srclen && (isalnum((unsigned char)src[at]) ||
	    src[at] == '_')) {
		if (n + 1 < buflen)
			buf[n++] = (char)tolower((unsigned char)src[at]);
		at++;
	}
	buf[n] = '\0';
	return (true);
}

/*
 * A four-character code in single quotes.  Shorter ones are padded, which
 * is how 'STR ' is usually written as 'STR'.
 */
static bool
take_code(char out[5])
{
	size_t n = 0;

	if (!take_char('\''))
		return (false);

	while (at < srclen && src[at] != '\'') {
		if (src[at] == '\n')
			return (false);
		if (n < 4)
			out[n++] = src[at];
		at++;
	}
	if (at >= srclen)
		return (false);
	at++;

	while (n < 4)
		out[n++] = ' ';
	out[4] = '\0';
	return (true);
}

static bool
take_string(char **out)
{
	char buf[1024];
	size_t n = 0;

	if (!take_char('"'))
		return (false);

	while (at < srclen && src[at] != '"') {
		char c = src[at];

		if (c == '\n') {
			diag("String spans lines.");
			return (false);
		}
		if (c == '\\' && at + 1 < srclen)
			c = src[++at];
		if (n + 1 < sizeof(buf))
			buf[n++] = c;
		at++;
	}
	if (at >= srclen)
		return (false);
	at++;

	buf[n] = '\0';
	*out = strdup(buf);
	return (*out != NULL);
}

static bool
take_number(long *out)
{
	char *end;
	long v;
	bool neg = false;

	skip_space();
	if (at < srclen && (src[at] == '-' || src[at] == '+')) {
		neg = (src[at] == '-');
		at++;
		skip_space();
	}
	if (at >= srclen || !isdigit((unsigned char)src[at]))
		return (false);

	v = strtol(src + at, &end, 0);
	at = (size_t)(end - src);
	*out = neg ? -v : v;
	return (true);
}

/* $"0102 0304", appended to a growing buffer. */
static bool
take_hex(uint8_t **data, uint32_t *len, size_t *cap)
{
	int hi = -1;

	if (!take_char('$'))
		return (false);
	if (!take_char('"')) {
		diag("Expected a string after '$'.");
		return (false);
	}

	while (at < srclen && src[at] != '"') {
		unsigned char c = (unsigned char)src[at];

		if (c == '\n') {
			diag("String spans lines.");
			return (false);
		}
		if (isspace(c)) {
			at++;
			continue;
		}
		if (!isxdigit(c)) {
			diag("Invalid character in hex string.");
			return (false);
		}

		{
			int v = isdigit(c) ? c - '0' :
			    (int)(tolower(c) - 'a' + 10);

			if (hi < 0) {
				hi = v;
			} else {
				if (*len + 1 > *cap) {
					size_t want = (*cap == 0) ? 64 : *cap * 2;
					uint8_t *p = realloc(*data, want);

					if (p == NULL)
						return (false);
					*data = p;
					*cap = want;
				}
				(*data)[(*len)++] = (uint8_t)((hi << 4) | v);
				hi = -1;
			}
		}
		at++;
	}
	if (at >= srclen)
		return (false);
	at++;

	if (hi >= 0) {
		diag("Odd number of digits in hex string.");
		return (false);
	}
	return (true);
}

/* --- parsing ------------------------------------------------------ */

static bool
parse_attributes(uint8_t *attrs)
{
	char word[64];

	for (;;) {
		if (!take_char(','))
			return (true);

		skip_space();
		if (peek_char('"'))
			return (true);	/* caller handles a name here */

		if (!take_word(word, sizeof(word))) {
			diag("Expected an attribute.");
			return (false);
		}

		if (strcmp(word, "sysheap") == 0)
			*attrs |= RES_SYSHEAP;
		else if (strcmp(word, "purgeable") == 0)
			*attrs |= RES_PURGEABLE;
		else if (strcmp(word, "locked") == 0)
			*attrs |= RES_LOCKED;
		else if (strcmp(word, "protected") == 0)
			*attrs |= RES_PROTECTED;
		else if (strcmp(word, "preload") == 0)
			*attrs |= RES_PRELOAD;
		else if (strcmp(word, "changed") == 0)
			*attrs |= RES_CHANGED;
		else if (strcmp(word, "appheap") == 0)
			*attrs &= (uint8_t)~RES_SYSHEAP;
		else if (strcmp(word, "nonpurgeable") == 0)
			*attrs &= (uint8_t)~RES_PURGEABLE;
		else if (strcmp(word, "unlocked") == 0)
			*attrs &= (uint8_t)~RES_LOCKED;
		else if (strcmp(word, "unprotected") == 0)
			*attrs &= (uint8_t)~RES_PROTECTED;
		else if (strcmp(word, "nonpreload") == 0)
			*attrs &= (uint8_t)~RES_PRELOAD;
		else if (strcmp(word, "unchanged") == 0)
			*attrs &= (uint8_t)~RES_CHANGED;
		else {
			diag("Unknown resource attribute (%s).", word);
			return (false);
		}
	}
}

/* ( id [, "name"] [, attribute]... ) */
static bool
parse_head(struct resource *r)
{
	long id;

	if (!take_char('(')) {
		diag("Expected '(' after the resource type.");
		return (false);
	}
	if (!take_number(&id)) {
		diag("Expected a resource ID.");
		return (false);
	}
	r->id = (int16_t)id;

	if (peek_char(',')) {
		size_t save = at;
		int saveline = line;

		(void)take_char(',');
		skip_space();
		if (peek_char('"')) {
			if (!take_string(&r->name))
				return (false);
			/*
			 * An empty name is no name: Apple's writes the
			 * "none" marker rather than a zero-length Pascal
			 * string, and their DeRez prints no name back.
			 */
			if (r->name[0] == '\0') {
				free(r->name);
				r->name = NULL;
			}
		} else {
			at = save;
			line = saveline;
		}
	}

	if (!parse_attributes(&r->attrs))
		return (false);

	if (!take_char(')')) {
		diag("Expected ')' after the resource ID.");
		return (false);
	}
	return (true);
}

static bool
parse_data(struct resource *r)
{
	size_t cap = 0;

	if (!take_char('{')) {
		diag("Expected '{' to start the data.");
		return (false);
	}

	while (!peek_char('}')) {
		if (at >= srclen) {
			diag("Unterminated data statement.");
			return (false);
		}
		if (!take_hex(&r->data, &r->length, &cap))
			return (false);
	}
	(void)take_char('}');
	return (true);
}

static bool
parse_read(struct resource *r)
{
	char *path = NULL;
	struct stat st;
	FILE *fp;

	if (!take_string(&path)) {
		diag("Expected a file name.");
		return (false);
	}

	fp = fopen(path, "rb");
	if (fp == NULL || stat(path, &st) != 0) {
		diag("Fatal Error: File \"%s\" could not be opened.", path);
		free(path);
		if (fp != NULL)
			(void)fclose(fp);
		return (false);
	}

	r->length = (uint32_t)st.st_size;
	r->data = malloc(r->length + 1);
	if (r->data == NULL || (r->length > 0 &&
	    fread(r->data, 1, r->length, fp) != r->length)) {
		diag("Fatal Error: File \"%s\" could not be read.", path);
		(void)fclose(fp);
		free(path);
		return (false);
	}
	(void)fclose(fp);
	free(path);
	return (true);
}

int
main(int argc, char *argv[])
{
	struct resource items[MAX_RESOURCES];
	const char *inputs[64];
	const char *out_path = NULL, *ftype = NULL, *fcreator = NULL;
	bool append = false, datafork = false;
	size_t ninputs = 0, count = 0, statement = 0, i;
	uint16_t fileref;
	uint8_t *fork;
	size_t forklen;

	for (i = 1; i < (size_t)argc; i++) {
		const char *a = argv[i];

		if (a[0] != '-') {
			if (ninputs < 64)
				inputs[ninputs++] = a;
		} else if (strcmp(a, "-o") == 0 && i + 1 < (size_t)argc) {
			out_path = argv[++i];
		} else if (strcmp(a, "-t") == 0 && i + 1 < (size_t)argc) {
			ftype = argv[++i];
		} else if (strcmp(a, "-c") == 0 && i + 1 < (size_t)argc) {
			fcreator = argv[++i];
		} else if (strcmp(a, "-a") == 0 || strcmp(a, "-append") == 0) {
			append = true;
		} else if (strcmp(a, "-useDF") == 0) {
			datafork = true;
		} else if (strcmp(a, "-ov") == 0 || strcmp(a, "-p") == 0 ||
		    strcmp(a, "-m") == 0 || strcmp(a, "-rd") == 0 ||
		    strcmp(a, "-ro") == 0) {
			/* accepted, nothing to do here */
		} else if (strcmp(a, "-d") == 0 || strcmp(a, "-i") == 0 ||
		    strcmp(a, "-isysroot") == 0 || strcmp(a, "-s") == 0 ||
		    strcmp(a, "-u") == 0 || strcmp(a, "-sc") == 0 ||
		    strcmp(a, "-arch") == 0 || strcmp(a, "-al") == 0 ||
		    strcmp(a, "-F") == 0 || strcmp(a, "-noResolve") == 0) {
			i++;			/* takes an argument */
		} else {
			(void)fprintf(stderr, "### %s - The option %s is not "
			    "yet implemented.\n", progname, a);
			usage();
			return (1);
		}
	}

	if (ninputs == 0) {
		(void)fprintf(stderr, "### %s - No filename to compile was "
		    "specified.\n", progname);
		usage();
		return (1);
	}
	if (out_path == NULL)
		out_path = "rez.out";

	memset(items, 0, sizeof(items));

	/*
	 * The reference number counts the resource files that had to be
	 * opened: one fewer when the destination is new, one fewer again
	 * without -a.  Apple's writes 0x0a00, 0x0900 or 0x0800 accordingly,
	 * and this arithmetic reproduces all four combinations.
	 */
	fileref = RESFORK_FILEREF_REZ;
	if (access(out_path, F_OK) == 0)
		fileref -= 0x0100;
	if (append)
		fileref -= 0x0100;

	/* -a keeps what the destination already holds, ahead of the input. */
	if (append && access(out_path, F_OK) == 0) {
		struct resfork old;
		const char *why = "";
		size_t len;
		uint8_t *bytes = resfork_read_file(out_path, datafork, &len);

		if (bytes != NULL && len > 0 &&
		    resfork_parse(bytes, len, &old, &why) == 0) {
			/*
			 * The spare four bytes hold a handle, and a handle
			 * exists only for a resource the Resource Manager
			 * actually loaded.  Opening a file loads the ones
			 * marked preload and nothing else, so those take the
			 * first numbers and the statements compiled below
			 * carry on from there.
			 */
			for (i = 0; i < old.count && count < MAX_RESOURCES; i++) {
				items[count] = old.items[i];
				items[count].reserved = 0;
				if ((items[count].attrs & RES_PRELOAD) != 0) {
					statement++;
					items[count].reserved =
					    (uint32_t)statement << 24;
				}
				count++;
			}
		}
	}

	for (i = 0; i < ninputs; i++) {
		char *text;
		long size;
		FILE *fp = fopen(inputs[i], "rb");

		srcname = inputs[i];
		line = 1;

		if (fp == NULL) {
			(void)fprintf(stderr, "### %s - Fatal Error: File "
			    "\"%s\" could not be opened.\n", progname,
			    inputs[i]);
			errors++;
			continue;
		}
		(void)fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		(void)fseek(fp, 0, SEEK_SET);
		text = malloc((size_t)size + 1);
		if (text == NULL || fread(text, 1, (size_t)size, fp) !=
		    (size_t)size) {
			(void)fclose(fp);
			free(text);
			errors++;
			continue;
		}
		text[size] = '\0';
		(void)fclose(fp);

		src = text;
		srclen = (size_t)size;
		at = 0;

		for (;;) {
			char word[64];
			struct resource *r;

			skip_space();
			if (at >= srclen)
				break;

			if (!take_word(word, sizeof(word))) {
				diag("Expected 'DATA', 'ENUM', 'INCLUDE', "
				    "'READ', 'RESOURCE', or 'TYPE'.");
				break;
			}

			if (strcmp(word, "type") == 0 ||
			    strcmp(word, "resource") == 0 ||
			    strcmp(word, "enum") == 0 ||
			    strcmp(word, "include") == 0) {
				diag("The '%s' statement is not yet "
				    "implemented; only 'data' and 'read' "
				    "are.", word);
				break;
			}
			if (strcmp(word, "data") != 0 &&
			    strcmp(word, "read") != 0) {
				diag("Expected 'DATA', 'ENUM', 'INCLUDE', "
				    "'READ', 'RESOURCE', or 'TYPE', but got "
				    "identifier (%s)", word);
				break;
			}
			if (count >= MAX_RESOURCES) {
				diag("Too many resources.");
				break;
			}

			r = &items[count];
			memset(r, 0, sizeof(*r));

			if (!take_code(r->type)) {
				diag("Expected a resource type.");
				break;
			}
			if (!parse_head(r))
				break;

			if (strcmp(word, "data") == 0) {
				if (!parse_data(r))
					break;
			} else if (!parse_read(r)) {
				break;
			}

			if (!take_char(';')) {
				diag("Expected ';' after the statement.");
				break;
			}
			/*
			 * The spare four bytes hold a handle, and the
			 * Resource Manager hands them out in the order it
			 * loads things.  Replacing a resource means loading
			 * the old one first, so that consumes a number --
			 * unless it was already loaded when the file was
			 * opened, which is what preload means.
			 */
			{
				struct resource fresh;
				size_t d;

				for (d = 0; d < count; d++) {
					if (items[d].id != r->id ||
					    memcmp(items[d].type, r->type,
					    4) != 0)
						continue;

					if (items[d].reserved == 0)
						statement++;
					break;
				}

				statement++;
				r->reserved = (uint32_t)statement << 24;
				fresh = *r;

				/*
				 * The replacement takes the newer one's
				 * place at the end.  The old entry is
				 * dropped without freeing: a resource read
				 * back from the destination points into the
				 * fork buffer rather than owning its bytes,
				 * and the two cannot be told apart here.
				 * ponytail: leak bounded by input size;
				 * track ownership in struct resource if Rez
				 * ever runs long.
				 */
				if (d < count) {
					memmove(&items[d], &items[d + 1],
					    (count - d - 1) * sizeof(items[0]));
					count--;
					items[count] = fresh;
				}
			}

			count++;
		}
	}

	if (errors > 0) {
		(void)fprintf(stderr, "%s: ### %s - Since errors occurred, "
		    "%s's resource fork was not completely updated.\n",
		    srcname, progname, out_path);
		return (1);
	}

	fork = resfork_build(items, count, fileref, &forklen);
	if (fork == NULL) {
		(void)fprintf(stderr, "### %s - Fatal Error: out of memory.\n",
		    progname);
		return (1);
	}
	if (resfork_write_file(out_path, datafork, fork, forklen) != 0) {
		(void)fprintf(stderr, "### %s - Fatal Error: File \"%s\" could "
		    "not be written.\n", progname, out_path);
		free(fork);
		return (1);
	}
	free(fork);

	if (ftype != NULL || fcreator != NULL) {
		uint8_t info[FINDERINFO_SIZE];

		if (fi_read(out_path, false, info) == 0) {
			if (ftype != NULL)
				memcpy(info, ftype, strnlen(ftype, 4));
			if (fcreator != NULL)
				memcpy(info + 4, fcreator, strnlen(fcreator, 4));
			(void)fi_write(out_path, false, info);
		}
	}

	return (0);
}
