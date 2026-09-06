/*
 * resfork.h -- reading a Macintosh resource fork.
 *
 * The format is a header naming two regions, a data area of length-prefixed
 * blobs and a map listing what is in it: a type list, a reference list per
 * type, and a pool of Pascal strings for the resources that have names.
 * Every offset in the map is relative to something different, which is most
 * of what the reader has to keep straight.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef REZ_RESFORK_H
#define REZ_RESFORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Resource attribute bits, as they sit in the reference list entry. */
#define RES_SYSHEAP	0x40
#define RES_PURGEABLE	0x20
#define RES_LOCKED	0x10
#define RES_PROTECTED	0x08
#define RES_PRELOAD	0x04
#define RES_CHANGED	0x02

struct resource {
	char		type[5];	/* four characters, NUL-terminated */
	int16_t		id;
	char		*name;		/* NULL when the resource has none */
	uint8_t		attrs;
	uint8_t		*data;
	uint32_t	length;
};

struct resfork {
	struct resource	*items;
	size_t		count;
};

/*
 * Parse a whole fork.  Returns 0, or -1 with a message in *why -- a
 * truncated or self-contradictory file is expected input here, not a bug,
 * so every offset is checked before it is followed.
 */
int	resfork_parse(const uint8_t *bytes, size_t len, struct resfork *out,
	    const char **why);
void	resfork_free(struct resfork *rf);

/* The fork of a file, or its data fork with datafork set. */
uint8_t	*resfork_read_file(const char *path, bool datafork, size_t *len);

#endif /* REZ_RESFORK_H */
