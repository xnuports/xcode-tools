/*
 * SplitForks -- write a file's resource fork and Finder info out beside it.
 *
 *	SplitForks [-s][-v][-u] <file or directory>
 *
 * The metadata a file carries out of band -- its resource fork and its
 * FinderInfo -- goes into an AppleDouble file named "._" plus the original
 * name, which is how it survives a copy onto a filesystem that has no idea
 * such things exist.  Given a directory the whole tree is walked.
 *
 * Only HFS volumes are handled, which is Apple's rule and not an arbitrary
 * one: the tool exists to move forks off a volume that has them, and it
 * declines anywhere else rather than writing a ._ file nothing asked for.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "finderinfo.h"

#define ADOUBLE_MAGIC		0x00051607u
#define ADOUBLE_VERSION		0x00020000u

#define ADOUBLE_ID_RESOURCE	2u
#define ADOUBLE_ID_FINDERINFO	9u

/* 4 magic + 4 version + 16 filler + 2 count, then 12 per entry. */
#define ADOUBLE_HEADER		(4 + 4 + 16 + 2)
#define ADOUBLE_ENTRY		12
#define ADOUBLE_ENTRIES		2
#define ADOUBLE_FINDERINFO_AT	(ADOUBLE_HEADER + ADOUBLE_ENTRY * ADOUBLE_ENTRIES)
#define ADOUBLE_RESOURCE_AT	(ADOUBLE_FINDERINFO_AT + FINDERINFO_SIZE)

static const char *progname = "SplitForks";
static bool verbose;
static bool strip;

static void
usage(void)
{
	(void)printf("usage: %s [-s][-v][-u] <file or directory>\n", progname);
	(void)printf("       -s  --  Strip resource fork from source after splitting\n");
	(void)printf("       -v  --  Verbose mode\n");
	(void)printf("       -u  --  Show usage\n");
}

/* Everything in an AppleDouble header is big-endian. */
static void
put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void
put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)v;
}

static bool
is_hfs(const char *path)
{
	struct statfs fs;

	if (statfs(path, &fs) != 0)
		return (false);

	return (strncmp(fs.f_fstypename, "hfs", sizeof(fs.f_fstypename)) == 0);
}

/*
 * The resource fork is reachable as an extended attribute, which saves
 * opening the ..namedfork path just to find out there is nothing there.
 */
static ssize_t
resource_size(const char *path)
{
	ssize_t n = getxattr(path, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0);

	return (n < 0 ? 0 : n);
}

static bool
finderinfo_is_empty(const uint8_t info[FINDERINFO_SIZE])
{
	size_t i;

	for (i = 0; i < FINDERINFO_SIZE; i++)
		if (info[i] != 0)
			return (false);

	return (true);
}

/*
 * Write ._<name> beside the file.  Both entries are always present, even
 * when one of them is empty: Apple's writes a resource entry of length zero
 * for a file that has only Finder info, and the header is a fixed shape.
 */
static int
split_one(const char *dir, const char *name)
{
	uint8_t info[FINDERINFO_SIZE];
	uint8_t header[ADOUBLE_FINDERINFO_AT];
	char source[PATH_MAX], target[PATH_MAX];
	uint8_t *resource = NULL;
	ssize_t rsrclen;
	FILE *out;
	size_t off;

	(void)snprintf(source, sizeof(source), "%s/%s", dir, name);

	if (fi_read(source, false, info) != 0)
		return (0);
	rsrclen = resource_size(source);

	/* Nothing out of band means nothing to write, and nothing to say. */
	if (rsrclen == 0 && finderinfo_is_empty(info))
		return (0);

	if (rsrclen > 0) {
		resource = malloc((size_t)rsrclen);
		if (resource == NULL)
			return (-1);
		if (getxattr(source, XATTR_RESOURCEFORK_NAME, resource,
		    (size_t)rsrclen, 0, 0) != rsrclen) {
			free(resource);
			return (-1);
		}
	}

	memset(header, 0, sizeof(header));
	off = 0;
	put32(header + off, ADOUBLE_MAGIC); off += 4;
	put32(header + off, ADOUBLE_VERSION); off += 4;
	off += 16;			/* filler, left zero */
	put16(header + off, ADOUBLE_ENTRIES); off += 2;

	put32(header + off, ADOUBLE_ID_FINDERINFO); off += 4;
	put32(header + off, ADOUBLE_FINDERINFO_AT); off += 4;
	put32(header + off, FINDERINFO_SIZE); off += 4;

	put32(header + off, ADOUBLE_ID_RESOURCE); off += 4;
	put32(header + off, ADOUBLE_RESOURCE_AT); off += 4;
	put32(header + off, (uint32_t)rsrclen);

	(void)snprintf(target, sizeof(target), "%s/._%s", dir, name);
	out = fopen(target, "wb");
	if (out == NULL) {
		free(resource);
		return (-1);
	}

	(void)fwrite(header, sizeof(header), 1, out);
	(void)fwrite(info, FINDERINFO_SIZE, 1, out);
	if (rsrclen > 0)
		(void)fwrite(resource, (size_t)rsrclen, 1, out);
	(void)fclose(out);
	free(resource);

	if (verbose)
		(void)printf("    splitting %s...\n", name);

	/*
	 * Removing the attribute rather than truncating the named fork.
	 * truncate(2) on ..namedfork/rsrc returns EPERM, and the result of
	 * removing it is what Apple's leaves behind anyway: the fork is
	 * still there to stat and reports zero length.
	 */
	if (strip && rsrclen > 0)
		(void)removexattr(source, XATTR_RESOURCEFORK_NAME, 0);

	return (0);
}

static int
split_tree(const char *path)
{
	struct stat st;
	struct dirent *ent;
	DIR *dir;
	int status = 0;

	if (lstat(path, &st) != 0)
		return (0);

	if (!S_ISDIR(st.st_mode)) {
		const char *slash = strrchr(path, '/');
		char parent[PATH_MAX];

		if (slash == NULL)
			return (split_one(".", path));

		(void)snprintf(parent, sizeof(parent), "%.*s",
		    (int)(slash - path), path);
		return (split_one(parent[0] == '\0' ? "/" : parent, slash + 1));
	}

	dir = opendir(path);
	if (dir == NULL)
		return (0);

	while ((ent = readdir(dir)) != NULL) {
		char child[PATH_MAX];

		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;

		(void)snprintf(child, sizeof(child), "%s/%s", path,
		    ent->d_name);

		if (ent->d_type == DT_DIR)
			status |= split_tree(child);
		else
			status |= split_one(path, ent->d_name);
	}
	(void)closedir(dir);

	return (status);
}

int
main(int argc, char *argv[])
{
	int i, status = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0')
			break;

		switch (argv[i][1]) {
		case 's':
			strip = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'u':
			usage();
			return (0);
		default:
			(void)printf("Invalid option %s\n", argv[i]);
			usage();
			return (0);
		}
	}

	/* Not the usage message: -u is how you ask for that. */
	if (i >= argc) {
		(void)printf("No file or directory was specified\n");
		return (0);
	}

	for (; i < argc; i++) {
		char resolved[PATH_MAX];
		struct stat st;

		if (lstat(argv[i], &st) != 0) {
			(void)printf("%s was not found\n", argv[i]);
			continue;
		}

		if (realpath(argv[i], resolved) == NULL)
			(void)strncpy(resolved, argv[i], sizeof(resolved) - 1);

		if (!is_hfs(argv[i])) {
			(void)printf("%s is not on an hfs disk\n", argv[i]);
			continue;
		}

		if (verbose)
			(void)printf("Splitting %s...\n", resolved);

		status |= split_tree(argv[i]);
	}

	return (status == 0 ? 0 : 1);
}
