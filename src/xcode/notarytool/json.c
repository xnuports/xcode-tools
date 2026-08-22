/*
 * json - minimal JSON string extraction for notarytool API responses.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Apple Notary Service API returns JSON:API responses. This parser
 * is deliberately minimal: it extracts string values by key name from
 * well-formed JSON, handling basic escaping. No full JSON document model
 * is built; values are extracted on-demand.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "json.h"

/*
 * Skip whitespace characters.
 */
static const char *
skip_ws(const char *p)
{
	while (*p && isspace((unsigned char)*p))
		p++;
	return p;
}

/*
 * Unescape a JSON string starting at *p (which should point to the
 * opening quote). Returns the unescaped string (malloc'd) and advances
 * *p past the closing quote.
 */
static char *
parse_json_string(const char **p)
{
	const char *s = *p;
	if (*s != '"')
		return NULL;
	s++;

	size_t cap = 32;
	size_t len = 0;
	char *out = malloc(cap);
	if (out == NULL)
		return NULL;

	while (*s && *s != '"') {
		if (*s == '\\' && s[1] != '\0') {
			char c = s[1];
			if (c == 'n') c = '\n';
			else if (c == 't') c = '\t';
			else if (c == 'r') c = '\r';
			else if (c == '\\') c = '\\';
			else if (c == '"') c = '"';
			else if (c == '/') c = '/';
			else if (c == 'b') c = '\b';
			else if (c == 'f') c = '\f';
			if (len + 1 >= cap) {
				cap *= 2;
				char *nd = realloc(out, cap);
				if (nd == NULL) {
					free(out);
					return NULL;
				}
				out = nd;
			}
			out[len++] = c;
			s += 2;
		} else {
			if (len + 1 >= cap) {
				cap *= 2;
				char *nd = realloc(out, cap);
				if (nd == NULL) {
					free(out);
					return NULL;
				}
				out = nd;
			}
			out[len++] = *s;
			s++;
		}
	}

	if (*s == '"')
		s++;
	out[len] = '\0';
	*p = s;
	return out;
}

/*
 * Extract a string value for a key from a JSON document.
 * Searches for "key" : "value" pattern.
 */
char *
json_extract_value(const char *json, const char *key)
{
	/* Build the search pattern: "key" */
	char pattern[256];
	snprintf(pattern, sizeof pattern, "\"%s\"", key);

	const char *kp = strstr(json, pattern);
	if (kp == NULL)
		return NULL;

	kp += strlen(pattern);
	kp = skip_ws(kp);

	/* Expect ':' */
	if (*kp != ':')
		return NULL;
	kp++;
	kp = skip_ws(kp);

	/* The value should be a string */
	return parse_json_string(&kp);
}

/*
 * Extract a string value for a given key, searching within a JSON
 * object. Finds the key and extracts its string value.
 */
char *
json_get_string(const char *json, const char *key)
{
	return json_extract_value(json, key);
}

/*
 * Extract a nested string: find key1's object, then key2 within it.
 */
char *
json_get_nested_string(const char *json, const char *key1, const char *key2)
{
	/* Find key1's value object */
	char pattern1[256];
	snprintf(pattern1, sizeof pattern1, "\"%s\"", key1);

	const char *kp = strstr(json, pattern1);
	if (kp == NULL)
		return NULL;

	kp += strlen(pattern1);
	kp = skip_ws(kp);
	if (*kp != ':')
		return NULL;
	kp++;
	kp = skip_ws(kp);

	/* If value is an object, search within it for key2 */
	if (*kp == '{') {
		/* Find the matching closing brace */
		const char *obj_start = kp;
		int depth = 0;
		int in_str = 0;
		const char *end = obj_start;
		while (*end && (depth > 0 || end == obj_start)) {
			if (in_str) {
				if (*end == '\\' && end[1] != '\0') {
					end += 2;
					continue;
				}
				if (*end == '"')
					in_str = 0;
				end++;
			} else {
				if (*end == '"')
					in_str = 1;
				else if (*end == '{')
					depth++;
				else if (*end == '}') {
					depth--;
					if (depth == 0) {
						end++;
						break;
					}
				}
				end++;
			}
		}

		/* Search for key2 within this object */
		char pattern2[256];
		snprintf(pattern2, sizeof pattern2, "\"%s\"", key2);

		const char *kp2 = strstr(obj_start, pattern2);
		if (kp2 == NULL || kp2 >= end)
			return NULL;

		kp2 += strlen(pattern2);
		kp2 = skip_ws(kp2);
		if (*kp2 != ':')
			return NULL;
		kp2++;
		kp2 = skip_ws(kp2);

		return parse_json_string(&kp2);
	}

	return NULL;
}
