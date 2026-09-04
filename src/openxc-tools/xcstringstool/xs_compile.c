/*
 * xs_compile - turn a string catalog back into per-language files.
 *
 * A catalog holds every language in one JSON document; the frameworks
 * load .strings and .stringsdict from <lang>.lproj at runtime, so
 * compiling means splitting the catalog by language and writing each
 * side out as a property list.
 *
 * Only units marked "translated" are written.  A key still being worked
 * on is carried in the catalog with state "new", and emitting it would
 * ship a draft as though it were a translation.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "xcstringstool.h"

static const char *program_name = "xcstringstool";

/*
 * Property lists are built and written with CoreFoundation, which is
 * what Apple's own xcstringstool links.  Serializing them any other way
 * means reproducing how CFPropertyList orders a dictionary's keys --
 * hash order in the binary form, which is stable but is not the source
 * order and not sorted -- and the output stops being byte-for-byte what
 * Apple writes.  Through CF it is exactly that, in both formats.
 */
static CFStringRef
cfstr(const char *s)
{
	return CFStringCreateWithCString(kCFAllocatorDefault,
	    (s != NULL) ? s : "", kCFStringEncodingUTF8);
}

/* Set one string value on a dictionary. */
static void
dict_set_string(CFMutableDictionaryRef dict, const char *key, const char *value)
{
	CFStringRef k = cfstr(key);
	CFStringRef v = cfstr(value);

	if (k != NULL && v != NULL)
		CFDictionarySetValue(dict, k, v);
	if (k != NULL)
		CFRelease(k);
	if (v != NULL)
		CFRelease(v);
}

static CFMutableDictionaryRef
dict_new(void)
{
	return CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

/* Write a property list out in the requested serialization. */
static int
write_plist(const char *path, CFDictionaryRef dict, int binary)
{
	CFPropertyListFormat fmt = binary ?
	    kCFPropertyListBinaryFormat_v1_0 : kCFPropertyListXMLFormat_v1_0;
	CFDataRef data;
	FILE *fp;
	size_t len, n;

	if ((data = CFPropertyListCreateData(kCFAllocatorDefault, dict, fmt, 0,
	    NULL)) == NULL)
		return -1;

	len = (size_t)CFDataGetLength(data);

	if ((fp = fopen(path, "wb")) == NULL) {
		CFRelease(data);
		return -1;
	}

	n = fwrite(CFDataGetBytePtr(data), 1, len, fp);
	fclose(fp);
	CFRelease(data);

	return (n == len) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* format specifiers                                                    */
/* ------------------------------------------------------------------ */

struct spec {
	size_t start;		/* offset of '%' */
	size_t len;		/* length including the conversion */
	int has_position;	/* already written as %N$... */
	int numeric;
	char type[8];		/* length modifier plus conversion, e.g. "lld" */
};

static int
is_numeric_conv(char c)
{
	return strchr("diufFeEgGaA", c) != NULL;
}

/*
 * Find the printf specifiers in a string.  "%%" is a literal percent and
 * not a specifier, which matters because a plural whose only '%' is an
 * escaped one references no number at all.
 */
static size_t
scan_specs(const char *s, struct spec *out, size_t max)
{
	size_t n = 0, i = 0;

	while (s[i] != '\0') {
		size_t start, tlen = 0;

		if (s[i] != '%') {
			i++;
			continue;
		}
		if (s[i + 1] == '%') {
			i += 2;
			continue;
		}

		start = i++;
		if (n >= max)
			return n;

		memset(&out[n], 0, sizeof(out[n]));
		out[n].start = start;

		/* Explicit argument position. */
		{
			size_t j = i;

			while (s[j] >= '0' && s[j] <= '9')
				j++;
			if (j > i && s[j] == '$') {
				out[n].has_position = 1;
				i = j + 1;
			}
		}

		while (strchr("-+ #0'", s[i]) != NULL && s[i] != '\0')
			i++;
		while (s[i] >= '0' && s[i] <= '9')
			i++;
		if (s[i] == '.') {
			i++;
			while (s[i] >= '0' && s[i] <= '9')
				i++;
		}

		/* Length modifier, kept: it is part of the value type. */
		while (strchr("hlLqjzt", s[i]) != NULL && s[i] != '\0') {
			if (tlen + 1 < sizeof(out[n].type))
				out[n].type[tlen++] = s[i];
			i++;
		}

		if (s[i] == '\0')
			break;

		if (tlen + 1 < sizeof(out[n].type))
			out[n].type[tlen++] = s[i];
		out[n].type[tlen] = '\0';
		out[n].numeric = is_numeric_conv(s[i]);
		i++;

		out[n].len = i - start;
		n++;
	}

	return n;
}

/*
 * Rewrite a variant with explicit argument positions.
 *
 * Needed only when a plural string carries more than one specifier: the
 * stringsdict entry has to name which argument the plural rule counts,
 * and once one specifier is numbered they all must be.  Caller frees.
 */
static char *
positionalize(const char *s, const struct spec *specs, size_t nspecs)
{
	size_t need = strlen(s) + nspecs * 8 + 1;
	char *out = malloc(need);
	size_t oi = 0, si = 0, k;

	if (out == NULL)
		return NULL;

	for (k = 0; k < nspecs; k++) {
		size_t head = specs[k].start - si;

		memcpy(out + oi, s + si, head);
		oi += head;
		si = specs[k].start;

		if (specs[k].has_position) {
			memcpy(out + oi, s + si, specs[k].len);
			oi += specs[k].len;
		} else {
			oi += (size_t)snprintf(out + oi, need - oi, "%%%zu$", k + 1);
			/* copy the specifier body, minus its leading '%' */
			memcpy(out + oi, s + si + 1, specs[k].len - 1);
			oi += specs[k].len - 1;
		}
		si += specs[k].len;
	}

	strcpy(out + oi, s + si);
	return out;
}

static int
mkdirs(const char *path)
{
	char buf[1024];
	size_t i;

	if (snprintf(buf, sizeof(buf), "%s", path) >= (int)sizeof(buf))
		return -1;

	for (i = 1; buf[i] != '\0'; i++) {
		if (buf[i] != '/')
			continue;
		buf[i] = '\0';
		if (mkdir(buf, 0777) != 0 && errno != EEXIST)
			return -1;
		buf[i] = '/';
	}

	if (mkdir(buf, 0777) != 0 && errno != EEXIST)
		return -1;

	return 0;
}

/* ------------------------------------------------------------------ */
/* catalog walking                                                      */
/* ------------------------------------------------------------------ */

/* A unit is usable only once someone has actually translated it. */
static const char *
translated_value(const json_node *unit)
{
	const json_node *su, *state, *value;

	if ((su = json_object_get(unit, "stringUnit")) == NULL)
		return NULL;

	state = json_object_get(su, "state");
	if (json_string(state) == NULL ||
	    strcmp(json_string(state), "translated") != 0)
		return NULL;

	value = json_object_get(su, "value");
	return json_string(value);
}

static const json_node *
plural_of(const json_node *loc)
{
	const json_node *variations = json_object_get(loc, "variations");

	return (variations != NULL) ?
	    json_object_get(variations, "plural") : NULL;
}

static int
cmp_str(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Every language named anywhere in the catalog, sorted. */
static size_t
collect_languages(const json_node *strings, const char **out, size_t max)
{
	size_t n = 0, i, j, k;

	for (i = 0; i < json_count(strings); i++) {
		const json_node *loc =
		    json_object_get(strings->items[i], "localizations");

		for (j = 0; j < json_count(loc); j++) {
			const char *lang = loc->items[j]->key;
			int seen = 0;

			for (k = 0; k < n; k++)
				if (strcmp(out[k], lang) == 0) {
					seen = 1;
					break;
				}
			if (!seen && n < max)
				out[n++] = lang;
		}
	}

	qsort(out, n, sizeof(*out), cmp_str);
	return n;
}

static int
wanted_language(const struct xs_compile_opts *opts, const char *lang)
{
	size_t i;

	if (opts->nlanguages == 0)
		return 1;

	for (i = 0; i < opts->nlanguages; i++)
		if (strcmp(opts->languages[i], lang) == 0)
			return 1;

	return 0;
}

/* Keys of the catalog, sorted, so output does not depend on file order. */
static size_t
collect_keys(const json_node *strings, const char **out, size_t max)
{
	size_t n = 0, i;

	for (i = 0; i < json_count(strings) && n < max; i++)
		out[n++] = strings->items[i]->key;

	qsort(out, n, sizeof(*out), cmp_str);
	return n;
}

/*
 * Build one language's entries into a dictionary.
 *
 * Which kinds are wanted depends on the file: a .strings holds the plain
 * values, a .stringsdict the plural ones -- except under --format
 * stringsdictOnly, where no .strings is produced at all and both kinds
 * go into the .stringsdict together.
 *
 * The document is built rather than written, so the XML and binary forms
 * come from one description.  Returns the number of entries added and
 * counts, in *errors, the plurals that do not reference a number: the
 * rule has nothing to count then, and the frameworks cannot select a
 * variant.  Every offender is reported, so one run names all the work.
 */
static long
build_entries(CFMutableDictionaryRef out, const json_node *strings, const char **keys,
    size_t nkeys, const char *lang, const char *input,
    int want_plain, int want_plural, int *errors)
{
	long added = 0;
	size_t i;

	for (i = 0; i < nkeys; i++) {
		const json_node *entry = json_object_get(strings, keys[i]);
		const json_node *loc =
		    json_object_get(json_object_get(entry, "localizations"), lang);
		const json_node *plural;
		struct spec specs[32];
		size_t nspecs, numeric_index = 0, c;
		const char *sample = NULL;
		CFMutableDictionaryRef pd, vd;
		char fmtkey[32];

		if (loc == NULL)
			continue;

		plural = plural_of(loc);

		/* --- a plain value --- */
		if (plural == NULL) {
			const char *value;

			if (!want_plain)
				continue;
			if ((value = translated_value(loc)) == NULL)
				continue;

			dict_set_string(out, keys[i], value);
			added++;
			continue;
		}

		/* --- a plural --- */
		if (!want_plural)
			continue;

		/* Any translated variant describes the specifiers. */
		for (c = 0; c < json_count(plural); c++) {
			const char *v = translated_value(plural->items[c]);

			if (v != NULL) {
				sample = v;
				break;
			}
		}
		if (sample == NULL)
			continue;

		nspecs = scan_specs(sample, specs, 32);
		for (c = 0; c < nspecs; c++)
			if (specs[c].numeric) {
				numeric_index = c + 1;
				break;
			}

		if (numeric_index == 0) {
			if (errors != NULL) {
				fprintf(stderr, "%s: error: Plural variation"
				    " requires referencing the number in the"
				    " string. To maintain grammatical"
				    " correctness for strings that do not"
				    " reference the number of items, use"
				    " separate top-level strings for one and"
				    " greater than one. (%s: %s)\n",
				    input, lang, keys[i]);
				(*errors)++;
			}
			continue;
		}

		if ((pd = dict_new()) == NULL || (vd = dict_new()) == NULL)
			return -1;

		if (nspecs > 1)
			snprintf(fmtkey, sizeof(fmtkey), "%%%zu$#@value@",
			    numeric_index);
		else
			snprintf(fmtkey, sizeof(fmtkey), "%%#@value@");

		dict_set_string(pd, "NSStringLocalizedFormatKey", fmtkey);
		dict_set_string(vd, "NSStringFormatSpecTypeKey",
		    "NSStringPluralRuleType");
		dict_set_string(vd, "NSStringFormatValueTypeKey",
		    specs[numeric_index - 1].type);

		for (c = 0; c < json_count(plural); c++) {
			const char *category = plural->items[c]->key;
			const char *v = translated_value(plural->items[c]);
			char *rewritten = NULL;

			if (v == NULL)
				continue;

			if (nspecs > 1) {
				struct spec vs[32];
				size_t vn = scan_specs(v, vs, 32);

				rewritten = positionalize(v, vs, vn);
			}

			dict_set_string(vd, category,
			    (rewritten != NULL) ? rewritten : v);
			free(rewritten);
		}

		{
			CFStringRef vk = cfstr("value");
			CFStringRef ek = cfstr(keys[i]);

			CFDictionarySetValue(pd, vk, vd);
			CFDictionarySetValue(out, ek, pd);
			CFRelease(vk);
			CFRelease(ek);
			CFRelease(vd);
			CFRelease(pd);
		}
		added++;
	}

	return added;
}

/* ------------------------------------------------------------------ */

int
xs_compile(const struct xs_compile_opts *opts)
{
	char *text;
	size_t len;
	json_node *root;
	const json_node *strings;
	const char *langs[256];
	const char *keys[4096];
	size_t nlangs, nkeys, li;
	char base[256];
	const char *slash, *dot;
	int rc = 0;

	if ((text = xs_read_file(opts->input, &len)) == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", program_name, opts->input);
		return 1;
	}

	root = json_parse(text, len);
	free(text);
	if (root == NULL) {
		fprintf(stderr, "%s: %s is not a well-formed string catalog\n",
		    program_name, opts->input);
		return 1;
	}

	if ((strings = json_object_get(root, "strings")) == NULL) {
		fprintf(stderr, "%s: %s has no \"strings\"\n",
		    program_name, opts->input);
		json_free(root);
		return 1;
	}

	/* Output files take the catalog's base name. */
	slash = strrchr(opts->input, '/');
	snprintf(base, sizeof(base), "%s", (slash != NULL) ? slash + 1 : opts->input);
	if ((dot = strrchr(base, '.')) != NULL && dot != base)
		*(char *)dot = '\0';

	nlangs = collect_languages(strings, langs, sizeof(langs) / sizeof(langs[0]));
	nkeys = collect_keys(strings, keys, sizeof(keys) / sizeof(keys[0]));

	for (li = 0; li < nlangs && rc == 0; li++) {
		const char *lang = langs[li];
		char dir[1024], path[1200];
		long n_strings, n_dict;
		int errors = 0;

		if (!wanted_language(opts, lang))
			continue;

		/*
		 * Under stringsdictOnly there is no .strings file and the
		 * plain values join the plurals in the .stringsdict.
		 */
		int dict_takes_plain = (opts->format == XS_STRINGSDICT_ONLY);
		CFMutableDictionaryRef sd = NULL, dd = NULL;

		snprintf(dir, sizeof(dir), "%s/%s.lproj", opts->output_dir, lang);

		if (!dict_takes_plain) {
			if ((sd = dict_new()) == NULL) {
				rc = 1;
				break;
			}
			n_strings = build_entries(sd, strings, keys, nkeys, lang,
			    opts->input, 1, 0, NULL);
		} else {
			n_strings = 0;
		}

		if ((dd = dict_new()) == NULL) {
			if (sd != NULL)
				CFRelease(sd);
			rc = 1;
			break;
		}
		n_dict = build_entries(dd, strings, keys, nkeys, lang,
		    opts->input, dict_takes_plain, 1, &errors);

		if (errors > 0 || n_strings < 0 || n_dict < 0) {
			if (sd != NULL)
				CFRelease(sd);
			CFRelease(dd);
			rc = 1;
			continue;	/* still report the other languages */
		}

		/* A language with nothing to say produces no file. */
		if (n_strings > 0) {
			snprintf(path, sizeof(path), "%s/%s.strings", dir, base);
			if (opts->dry_run) {
				printf("%s\n", path);
			} else if (mkdirs(dir) != 0 ||
			    write_plist(path, sd, opts->binary) != 0) {
				fprintf(stderr, "%s: cannot write %s\n",
				    program_name, path);
				rc = 1;
			}
		}

		if (n_dict > 0 && rc == 0) {
			snprintf(path, sizeof(path), "%s/%s.stringsdict", dir, base);
			if (opts->dry_run) {
				printf("%s\n", path);
			} else if (mkdirs(dir) != 0 ||
			    write_plist(path, dd, opts->binary) != 0) {
				fprintf(stderr, "%s: cannot write %s\n",
				    program_name, path);
				rc = 1;
			}
		}

		if (sd != NULL)
			CFRelease(sd);
		CFRelease(dd);
	}

	json_free(root);
	return rc;
}

int
xs_print(const char *path)
{
	char *text;
	size_t len;
	json_node *root;
	const json_node *strings;
	const char *keys[4096];
	size_t nkeys, i;

	if ((text = xs_read_file(path, &len)) == NULL) {
		fprintf(stderr, "%s: cannot read %s\n", program_name, path);
		return 1;
	}

	root = json_parse(text, len);
	free(text);
	if (root == NULL) {
		fprintf(stderr, "%s: %s is not a well-formed string catalog\n",
		    program_name, path);
		return 1;
	}

	if ((strings = json_object_get(root, "strings")) == NULL) {
		fprintf(stderr, "%s: %s has no \"strings\"\n", program_name, path);
		json_free(root);
		return 1;
	}

	/*
	 * Sorted.  Apple's own order varies between runs of the same file;
	 * a build product should not.
	 */
	nkeys = collect_keys(strings, keys, sizeof(keys) / sizeof(keys[0]));
	for (i = 0; i < nkeys; i++)
		printf("%s\n", keys[i]);

	json_free(root);
	return 0;
}
