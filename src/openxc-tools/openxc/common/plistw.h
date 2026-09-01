/*
 * plistw.h -- build a property list and write it out.
 *
 * plist.h reads; this writes.  A document is built once and can then be
 * serialized either way, so the XML and binary forms of the same file
 * cannot describe different things -- which is the only real risk in
 * having two serializers.
 *
 * Keys are written in sorted order.  CoreFoundation writes them in hash
 * order, which is stable for a given input but is neither the source
 * order nor alphabetical and cannot be reproduced without reproducing
 * its hash; sorted is deterministic, and the two documents are equal as
 * property lists either way.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLISTW_H__
#define __PLISTW_H__

#include <stdio.h>
#include <stddef.h>

typedef struct pw_node pw_node;

/* Construction.  A node is owned by the parent it is added to. */
pw_node *pw_dict(void);
pw_node *pw_string(const char *value);

/* Add a member.  Takes ownership of value; returns 0 on success. */
int pw_dict_set(pw_node *dict, const char *key, pw_node *value);

void pw_free(pw_node *node);

/* Serialization.  Returns 0 on success. */
int pw_write_xml(FILE *fp, const pw_node *root);
int pw_write_binary(FILE *fp, const pw_node *root);

#endif /* __PLISTW_H__ */
