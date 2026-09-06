/*
 * bkeyed.h -- just enough binary plist to read an NSKeyedArchiver graph.
 *
 * The shared plist parser in common/ renders every scalar as a string,
 * which suits the settings files it was written for and cannot express a
 * keyed archive: that is a flat $objects array whose entries refer to one
 * another by UID, and the numbers in it are the data.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BKEYED_H
#define BKEYED_H

#include <stddef.h>
#include <stdint.h>

enum bk_kind {
	BK_NULL, BK_BOOL, BK_INT, BK_REAL, BK_DATA, BK_STRING, BK_ARRAY,
	BK_DICT, BK_UID
};

struct bk_value;

struct bk_pair {
	struct bk_value	*key;
	struct bk_value	*value;
};

struct bk_value {
	enum bk_kind	 kind;
	int64_t		 integer;	/* BK_INT, BK_UID, BK_BOOL */
	double		 real;
	char		*string;
	unsigned char	*data;
	size_t		 length;	/* bytes for DATA, count otherwise */
	struct bk_value	**items;	/* BK_ARRAY */
	struct bk_pair	*pairs;		/* BK_DICT */
};

/* Parse a binary plist.  Returns the root, or NULL. */
struct bk_value	*bk_parse(const unsigned char *bytes, size_t len);
void		 bk_free(struct bk_value *v);

const struct bk_value	*bk_dict_get(const struct bk_value *d, const char *key);

#endif /* BKEYED_H */
