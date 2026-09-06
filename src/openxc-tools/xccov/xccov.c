/*
 * xccov -- report the code coverage recorded in a result bundle.
 *
 *	xccov view --report [--json] <bundle.xcresult>
 *
 * The coverage report is stored in the bundle as an NSKeyedArchiver plist,
 * reachable from the root object through actionResult.coverage.reportRef.
 * Its classes are XCTHarness's -- XCTHCodeCoverage, and a target, file and
 * function beneath it -- which is why Apple's xccov links that framework.
 * Reading the archive needs none of it.
 *
 * A file's functions are not archived objects but a packed blob: a count,
 * then a record apiece of little-endian words, then the names one after
 * another with a NUL between.  The field order was settled by finding a
 * function that was never called, whose covered count is zero where its
 * executable count is one.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bkeyed.h"
#include "xcresult.h"

/* Words per function record, and where each field sits. */
#define FN_RECORD_WORDS	6
#define FN_COVERED	0
#define FN_EXECUTABLE	1
#define FN_LINE		2
#define FN_COUNT_LOW	4	/* the execution count is a 64-bit pair */
#define FN_COUNT_HIGH	5

static const struct bk_value	*objects;

/* $objects entries refer to one another by UID; this follows one. */
static const struct bk_value *
deref(const struct bk_value *v)
{
	if (v == NULL || v->kind != BK_UID || objects == NULL)
		return (v);
	if ((size_t)v->integer >= objects->length)
		return (NULL);
	return (objects->items[v->integer]);
}

static const struct bk_value *
field(const struct bk_value *d, const char *key)
{
	return (deref(bk_dict_get(d, key)));
}

static long long
field_int(const struct bk_value *d, const char *key)
{
	const struct bk_value *v = field(d, key);

	return (v != NULL && v->kind == BK_INT ? (long long)v->integer : 0);
}

static const char *
field_string(const struct bk_value *d, const char *key)
{
	const struct bk_value *v = field(d, key);

	return (v != NULL && v->kind == BK_STRING ? v->string : "");
}

static void
print_string(const char *s)
{
	(void)putchar('"');
	for (; *s != '\0'; s++) {
		unsigned char c = (unsigned char)*s;

		switch (c) {
		case '"':  (void)fputs("\\\"", stdout); break;
		case '\\': (void)fputs("\\\\", stdout); break;
		case '/':  (void)fputs("\\/", stdout); break;
		case '\n': (void)fputs("\\n", stdout); break;
		case '\t': (void)fputs("\\t", stdout); break;
		default:
			if (c < 0x20)
				(void)printf("\\u%04x", c);
			else
				(void)putchar((char)c);
		}
	}
	(void)putchar('"');
}

/*
 * Seventeen significant digits, which is what JSONSerialization writes and
 * so what Apple's output carries: 0.875 stays 0.875 because it is exactly
 * representable, while 0.9 comes out 0.90000000000000002.  A whole ratio
 * still prints as 1.
 */
static void
print_coverage(long long covered, long long executable)
{
	double ratio = executable > 0 ? (double)covered / (double)executable : 0;

	(void)printf("%.17g", ratio);
}

static void
print_functions(const struct bk_value *file)
{
	const struct bk_value *holder = field(file, "functions");
	const struct bk_value *blob;
	const unsigned char *b;
	const char *names;
	uint32_t count, i;
	size_t off;

	(void)fputs("\"functions\":[", stdout);

	blob = holder != NULL ? deref(bk_dict_get(holder, "NS.data")) : NULL;
	if (blob == NULL || blob->kind != BK_DATA || blob->length < 4) {
		(void)fputc(']', stdout);
		return;
	}

	b = blob->data;
	count = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) |
	    ((uint32_t)b[3] << 24));

	off = 4 + (size_t)count * FN_RECORD_WORDS * 4;
	if (off > blob->length) {
		(void)fputc(']', stdout);
		return;
	}
	names = (const char *)b + off;

	for (i = 0; i < count; i++) {
		const unsigned char *rec = b + 4 +
		    (size_t)i * FN_RECORD_WORDS * 4;
		uint32_t w[FN_RECORD_WORDS];
		unsigned long long execution;
		size_t k;

		for (k = 0; k < FN_RECORD_WORDS; k++)
			w[k] = (uint32_t)(rec[k * 4] | (rec[k * 4 + 1] << 8) |
			    (rec[k * 4 + 2] << 16) |
			    ((uint32_t)rec[k * 4 + 3] << 24));

		execution = ((unsigned long long)w[FN_COUNT_HIGH] << 32) |
		    w[FN_COUNT_LOW];

		if (i > 0)
			(void)fputc(',', stdout);

		(void)printf("{\"coveredLines\":%u,\"executableLines\":%u,"
		    "\"executionCount\":%llu,\"lineCoverage\":",
		    w[FN_COVERED], w[FN_EXECUTABLE], execution);
		print_coverage(w[FN_COVERED], w[FN_EXECUTABLE]);
		(void)printf(",\"lineNumber\":%u,\"name\":", w[FN_LINE]);

		/* The names run one after another, NUL between. */
		print_string(names);
		while (names < (const char *)b + blob->length && *names != '\0')
			names++;
		if (names < (const char *)b + blob->length)
			names++;

		(void)fputc('}', stdout);
	}
	(void)fputc(']', stdout);
}

static void
print_report(const struct bk_value *root)
{
	const struct bk_value *targets;
	long long covered = field_int(root, "coveredLines");
	long long executable = field_int(root, "executableLines");
	size_t t;

	(void)printf("{\"coveredLines\":%lld,\"executableLines\":%lld,"
	    "\"lineCoverage\":", covered, executable);
	print_coverage(covered, executable);
	(void)fputs(",\"targets\":[", stdout);

	targets = field(root, "buildableCoverageObjects");
	targets = targets != NULL ? deref(bk_dict_get(targets, "NS.objects"))
	    : NULL;

	for (t = 0; targets != NULL && t < targets->length; t++) {
		const struct bk_value *target = deref(targets->items[t]);
		const struct bk_value *files;
		long long tc = field_int(target, "coveredLines");
		long long te = field_int(target, "executableLines");
		size_t f;

		if (t > 0)
			(void)fputc(',', stdout);

		(void)fputs("{\"buildProductPath\":", stdout);
		print_string(field_string(target, "productPath"));
		(void)printf(",\"coveredLines\":%lld,\"executableLines\":%lld,"
		    "\"files\":[", tc, te);

		files = field(target, "sourceFiles");
		files = files != NULL ? deref(bk_dict_get(files, "NS.objects"))
		    : NULL;

		for (f = 0; files != NULL && f < files->length; f++) {
			const struct bk_value *file = deref(files->items[f]);
			long long fc = field_int(file, "coveredLines");
			long long fe = field_int(file, "executableLines");

			if (f > 0)
				(void)fputc(',', stdout);

			(void)printf("{\"coveredLines\":%lld,"
			    "\"executableLines\":%lld,", fc, fe);
			print_functions(file);
			(void)fputs(",\"lineCoverage\":", stdout);
			print_coverage(fc, fe);
			(void)fputs(",\"name\":", stdout);
			print_string(field_string(file, "name"));
			(void)fputs(",\"path\":", stdout);
			print_string(field_string(file, "documentLocation"));
			(void)fputc('}', stdout);
		}

		(void)fputs("],\"lineCoverage\":", stdout);
		print_coverage(tc, te);
		(void)fputs(",\"name\":", stdout);
		print_string(field_string(target, "name"));
		(void)fputc('}', stdout);
	}

	(void)fputs("]}", stdout);
}

/* The report's id, from actions[0].actionResult.coverage.reportRef. */
static char *
report_id(const char *bundle)
{
	struct xcresult_node *root;
	const struct xcresult_node *actions, *values;
	char *root_id = xcresult_root_id(bundle);
	char *out = NULL;
	int bad = -1;
	size_t i;

	if (root_id == NULL)
		return (NULL);

	root = xcresult_load(bundle, root_id, &bad);
	free(root_id);
	if (root == NULL)
		return (NULL);

	actions = xcresult_get(root, "actions");
	values = actions;
	for (i = 0; values != NULL && i < values->nvalues; i++) {
		const struct xcresult_node *action = values->values[i];
		const struct xcresult_node *result =
		    xcresult_get(action, "actionResult");
		const struct xcresult_node *coverage =
		    xcresult_get(result, "coverage");
		const struct xcresult_node *ref =
		    xcresult_get(coverage, "reportRef");
		const char *id = xcresult_reference_id(ref);

		if (id != NULL) {
			out = strdup(id);
			break;
		}
	}
	return (out);
}

int
main(int argc, char *argv[])
{
	const char *path = NULL;
	struct bk_value *plist;
	const struct bk_value *top, *root;
	unsigned char *bytes;
	char *id;
	size_t len;
	bool json = false, report = false;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--json") == 0)
			json = true;
		else if (strcmp(argv[i], "--report") == 0)
			report = true;
		else if (strcmp(argv[i], "view") == 0)
			continue;
		else if (argv[i][0] != '-')
			path = argv[i];
	}

	if (!report || path == NULL) {
		(void)fprintf(stderr,
		    "xccov view --report [--json] <bundle.xcresult>\n");
		return (1);
	}

	id = report_id(path);
	if (id == NULL) {
		(void)fprintf(stderr, "Error: Error Domain=XCCovErrorDomain "
		    "Code=0 \"Failed to load coverage report\"\n");
		return (1);
	}

	bytes = xcresult_object_bytes(path, id, &len);
	free(id);
	if (bytes == NULL) {
		(void)fprintf(stderr, "Error: could not read the coverage "
		    "report.\n");
		return (1);
	}

	plist = bk_parse(bytes, len);
	free(bytes);
	if (plist == NULL) {
		(void)fprintf(stderr, "Error: the coverage report is not a "
		    "property list.\n");
		return (1);
	}

	objects = bk_dict_get(plist, "$objects");
	top = bk_dict_get(plist, "$top");
	root = deref(bk_dict_get(top, "root"));

	/*
	 * No trailing newline: Apple's writes the object and stops, which
	 * matters to anything comparing the bytes.
	 */
	(void)json;
	print_report(root);

	bk_free(plist);
	return (0);
}
