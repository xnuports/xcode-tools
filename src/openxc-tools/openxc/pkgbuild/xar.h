/*
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef XAR_H
#define XAR_H

#include <stddef.h>
#include <stdint.h>

/* One entry in a xar archive heap. */
struct xar_entry {
	const char *name;       /* path within the archive, e.g. "Payload" */
	const char *encoding;   /* "application/x-gzip" or "application/octet-stream" */
	const void *data;       /* raw, uncompressed bytes */
	size_t size;            /* uncompressed size of data */
	int compressed;         /* 1: zlib-compress when packing, 0: store raw */
};

/*
 * Build a pkgutil-valid xar archive from the given entries.
 * The archive checksum is sha1(toc_zlib); the layout matches a real
 * pkgbuild-produced package (creation-time, heap-relative <offset>s).
 * Returns a malloc'd buffer (caller frees) and sets *out_len.
 */
unsigned char *xar_build(const struct xar_entry *entries, int n,
    size_t *out_len);

/*
 * Read an existing xar package and print its table of contents
 * (file names + sizes). Returns 0 on success, -1 on a corrupt archive.
 */
int xar_dump(const unsigned char *pkg, size_t len);

#endif /* XAR_H */
