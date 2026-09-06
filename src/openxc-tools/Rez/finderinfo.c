/*
 * finderinfo.c -- see finderinfo.h.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "finderinfo.h"

#include <sys/stat.h>
#include <sys/xattr.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#define XATTR_FINDERINFO	"com.apple.FinderInfo"

/*
 * The order here is the order the tools print, which is neither the bit
 * order nor alphabetical.
 */
const struct fi_attr fi_attrs[] = {
	{ 'a', FINDERINFO_FLAGS,  0x8000 },	/* alias */
	{ 'v', FINDERINFO_FLAGS,  0x4000 },	/* invisible */
	{ 'b', FINDERINFO_FLAGS,  0x2000 },	/* has bundle */
	{ 's', FINDERINFO_FLAGS,  0x1000 },	/* name locked */
	{ 't', FINDERINFO_FLAGS,  0x0800 },	/* stationery */
	{ 'c', FINDERINFO_FLAGS,  0x0400 },	/* custom icon */
	{ 'l', FI_ATTR_LOCKED,    0	   },	/* locked -- chflags, not here */
	{ 'i', FINDERINFO_FLAGS,  0x0100 },	/* has been inited */
	{ 'n', FINDERINFO_FLAGS,  0x0080 },	/* has no INITs */
	{ 'm', FINDERINFO_FLAGS,  0x0040 },	/* shared */
	{ 'e', FINDERINFO_FLAGS,  0x0010 },	/* hidden extension */
	{ 'd', FINDERINFO_FLAGS,  0x0001 },	/* on desktop */
	{ 'z', FINDERINFO_XFLAGS, 0x0080 },	/* in the extended word */
};

const size_t fi_nattrs = sizeof(fi_attrs) / sizeof(fi_attrs[0]);

const struct fi_attr *
fi_attr_find(char letter)
{
	size_t i;

	if (letter >= 'A' && letter <= 'Z')
		letter = (char)(letter - 'A' + 'a');

	for (i = 0; i < fi_nattrs; i++)
		if (fi_attrs[i].letter == letter)
			return (&fi_attrs[i]);

	return (NULL);
}

/* Both words are big-endian, as they were on the machine this format is
 * from. */
static uint16_t
word_get(const uint8_t *info, size_t offset)
{
	return ((uint16_t)((info[offset] << 8) | info[offset + 1]));
}

static void
word_set(uint8_t *info, size_t offset, uint16_t value)
{
	info[offset] = (uint8_t)(value >> 8);
	info[offset + 1] = (uint8_t)(value & 0xff);
}

int
fi_read(const char *path, bool nofollow, uint8_t info[FINDERINFO_SIZE])
{
	ssize_t n;

	memset(info, 0, FINDERINFO_SIZE);

	n = getxattr(path, XATTR_FINDERINFO, info, FINDERINFO_SIZE, 0,
	    nofollow ? XATTR_NOFOLLOW : 0);
	if (n >= 0)
		return (0);

	/*
	 * Having no FinderInfo is not a failure: an ordinary file has none
	 * until something sets one, and both tools report that as a type and
	 * creator of four NULs rather than as an error.
	 */
	if (errno == ENOATTR)
		return (0);

	return (-1);
}

int
fi_write(const char *path, bool nofollow, const uint8_t info[FINDERINFO_SIZE])
{
	return (setxattr(path, XATTR_FINDERINFO, info, FINDERINFO_SIZE, 0,
	    nofollow ? XATTR_NOFOLLOW : 0));
}

bool
fi_attr_get(const uint8_t info[FINDERINFO_SIZE], unsigned long bsdflags,
    const struct fi_attr *a)
{
	if (a->offset == FI_ATTR_LOCKED)
		return ((bsdflags & UF_IMMUTABLE) != 0);

	return ((word_get(info, a->offset) & a->mask) != 0);
}

void
fi_attr_set(uint8_t info[FINDERINFO_SIZE], const struct fi_attr *a, bool on)
{
	uint16_t word;

	if (a->offset == FI_ATTR_LOCKED)
		return;			/* the caller does chflags(2) */

	word = word_get(info, a->offset);
	if (on)
		word |= a->mask;
	else
		word &= (uint16_t)~a->mask;
	word_set(info, a->offset, word);
}

void
fi_format_code(const uint8_t *code, char *out, size_t outlen)
{
	char buf[4 * 5 + 1];		/* vis(3) needs four bytes per byte */

	/*
	 * A counted encode rather than strvis: the code is four bytes and is
	 * not terminated, and an unset one is four NULs, which strvis would
	 * read as the empty string.  strnvisx over strvisx because the latter
	 * cannot be told how big its output buffer is.
	 */
	(void)strnvisx(buf, sizeof(buf), (const char *)code, 4, VIS_CSTYLE);
	(void)strncpy(out, buf, outlen - 1);
	out[outlen - 1] = '\0';
}
