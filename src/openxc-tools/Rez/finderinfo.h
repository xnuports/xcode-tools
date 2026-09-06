/*
 * finderinfo.h -- the Finder metadata the resource tools share.
 *
 * GetFileInfo and SetFile both read and write com.apple.FinderInfo, the
 * 32-byte extended attribute holding what used to be a catalog entry's
 * FInfo and FXInfo: a four-character type and creator, and two flag words.
 *
 * One of the attribute letters is not in there at all.  "l", locked, is the
 * BSD user-immutable flag, which is why setting it stops you deleting the
 * file rather than merely marking it.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef REZ_FINDERINFO_H
#define REZ_FINDERINFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FINDERINFO_SIZE		32

/* Offsets of the two flag words within it: FInfo.fdFlags and
 * FXInfo.fdXFlags. */
#define FINDERINFO_FLAGS	8
#define FINDERINFO_XFLAGS	24

/*
 * One attribute as the tools spell it.  The letters and their bits were
 * taken from Apple's SetFile a letter at a time, rather than from a header:
 * set one, read the attribute back, and see which bit moved.
 */
struct fi_attr {
	char		letter;		/* lowercase form */
	size_t		offset;		/* flag word, or FI_ATTR_LOCKED */
	uint16_t	mask;
};

/* "l" is not a FinderInfo bit; it is chflags(2). */
#define FI_ATTR_LOCKED		((size_t)-1)

/* In the order the tools print them: avbstclinmedz. */
extern const struct fi_attr	fi_attrs[];
extern const size_t		fi_nattrs;

const struct fi_attr	*fi_attr_find(char letter);

/* Both return 0 or -1 with errno set.  A file with no FinderInfo reads as
 * all zeroes rather than as an error, which is what the tools report. */
int	fi_read(const char *path, bool nofollow, uint8_t info[FINDERINFO_SIZE]);
int	fi_write(const char *path, bool nofollow, const uint8_t info[FINDERINFO_SIZE]);

bool	fi_attr_get(const uint8_t info[FINDERINFO_SIZE], unsigned long bsdflags,
	    const struct fi_attr *a);
void	fi_attr_set(uint8_t info[FINDERINFO_SIZE], const struct fi_attr *a, bool on);

/* A four-character code as the tools print it: vis(3) in its C style, which
 * is where "\0\0\0\0" for an empty type and "\M^?" for 0xff come from. */
void	fi_format_code(const uint8_t *code, char *out, size_t outlen);

#endif /* REZ_FINDERINFO_H */
