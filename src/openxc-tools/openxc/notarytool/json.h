/*
 * notarytool - open source reimplementation of Apple's notarytool(1).
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef JSON_H
#define JSON_H

#include <stddef.h>

/*
 * Minimal JSON value extraction for notarytool API responses.
 * These functions search for string values in a JSON document.
 */

/*
 * Extract a string value for a given key from a JSON document.
 * Searches at any nesting level. Returns a malloc'd string or NULL.
 */
char *json_get_string(const char *json, const char *key);

/*
 * Extract a nested string value: first find key1, then key2 within that
 * object. Returns a malloc'd string or NULL.
 */
char *json_get_nested_string(const char *json, const char *key1,
    const char *key2);

/*
 * Extract a string field from a JSON object value: finds "key":"value".
 * The search starts from the position of "key".
 * Returns a malloc'd string or NULL.
 */
char *json_extract_value(const char *json, const char *key);

#endif /* JSON_H */