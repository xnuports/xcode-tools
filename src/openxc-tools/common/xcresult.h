/*
 * xcresult.h -- reading the objects in a .xcresult bundle.
 *
 * The bundle is a content-addressed store: Info.plist names a root object,
 * and each object sits in Data/data.<id>, usually zstd-compressed.  Most
 * hold a small self-describing serialisation; some -- logs, coverage
 * reports, attachments -- are whatever they are, and are handed back raw.
 *
 * The format is Apple's and undocumented; it was worked out from bundles
 * their tools produced.  See xcresult.c for the grammar.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef XCRESULT_H
#define XCRESULT_H

#include <stdbool.h>
#include <stddef.h>

struct xcresult_node;

struct xcresult_member {
	char			*key;
	struct xcresult_node	*value;	/* NULL when the member is text */
	char			*text;
};

struct xcresult_node {
	char			*type;		/* the type's name */
	struct xcresult_node	*supertype;	/* the type it derives from */
	struct xcresult_member	*members;
	size_t			 nmembers;
	struct xcresult_node	**values;	/* Array elements */
	size_t			 nvalues;
};

/* The id of the bundle's root object, from Info.plist.  Caller frees. */
char	*xcresult_root_id(const char *bundle);

/*
 * The bytes of one object, decompressed if it was compressed.  Caller
 * frees.  Not every object is a structured one, which is why this is
 * separate from parsing.
 */
unsigned char *xcresult_object_bytes(const char *bundle, const char *id,
	    size_t *len);

/*
 * Parse a structured object.  Returns NULL and sets *bad_byte to the byte
 * that was not a token when the bytes are not one of these.
 */
struct xcresult_node *xcresult_parse(const unsigned char *bytes, size_t len,
	    int *bad_byte);

/* Load and parse in one step. */
struct xcresult_node *xcresult_load(const char *bundle, const char *id,
	    int *bad_byte);

/* The value of a member, or NULL. */
const struct xcresult_node *xcresult_get(const struct xcresult_node *n,
	    const char *key);
const char *xcresult_get_text(const struct xcresult_node *n, const char *key);

/*
 * The id a Reference points at: the _v of its "id" member.  Returns NULL if
 * this is not shaped like a reference.
 */
const char *xcresult_reference_id(const struct xcresult_node *n);

#endif /* XCRESULT_H */
