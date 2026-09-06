/*
 * GetFileInfo -- report a file's Finder metadata.
 *
 *	GetFileInfo [-P] [-a[<attrib-letter>] | -t | -c | -d | -m] <path>
 *
 * With no option it prints every field; with one it prints that field
 * alone.  A directory has no type or creator, and those two lines are left
 * out for one -- as is the word "file", which becomes "directory".
 *
 * The paths printed are resolved: Apple's prints the absolute path however
 * the file was named on the command line.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "finderinfo.h"

/*
 * fnfErr.  These tools answer in Carbon result codes rather than errno,
 * which is why a missing file is -43 and not ENOENT.
 */
#define ERR_FILE_NOT_FOUND	(-43)

static const char *progname = "GetFileInfo";

static void
usage(void)
{
	(void)fprintf(stdout,
	    "usage: %s [-P] [-a[<attrib-letter>] | -t | -c | -d | -m] <path>\n",
	    progname);
}

/* mm/dd/yyyy hh:mm:ss, in local time. */
static void
format_time(time_t when, char *out, size_t outlen)
{
	struct tm tm;

	(void)localtime_r(&when, &tm);
	(void)strftime(out, outlen, "%m/%d/%Y %H:%M:%S", &tm);
}

static void
print_attributes(const uint8_t *info, unsigned long bsdflags)
{
	size_t i;

	/*
	 * One letter per attribute, in the fixed order: uppercase when set,
	 * lowercase when clear.
	 */
	for (i = 0; i < fi_nattrs; i++) {
		char c = fi_attrs[i].letter;

		if (fi_attr_get(info, bsdflags, &fi_attrs[i]))
			c = (char)(c - 'a' + 'A');
		(void)putchar(c);
	}
	(void)putchar('\n');
}

int
main(int argc, char *argv[])
{
	uint8_t info[FINDERINFO_SIZE];
	char resolved[PATH_MAX];
	char code[32], stamp[64];
	const struct fi_attr *attr = NULL;
	const char *path;
	struct stat st;
	bool nofollow = false;
	int field = 0;			/* 0 = every field */
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0')
			break;

		switch (argv[i][1]) {
		case 'P':
			nofollow = true;
			break;
		case 'a':
			field = 'a';
			/* -a takes an optional letter with no space. */
			if (argv[i][2] != '\0') {
				attr = fi_attr_find(argv[i][2]);
				if (attr == NULL) {
					usage();
					return (1);
				}
			}
			break;
		case 't':
		case 'c':
		case 'd':
		case 'm':
			field = argv[i][1];
			break;
		default:
			usage();
			return (1);
		}
	}

	if (i >= argc) {
		usage();
		return (0);
	}
	path = argv[i];

	if ((nofollow ? lstat(path, &st) : stat(path, &st)) != 0) {
		(void)fprintf(stdout, "%s: could not refer to file (%d)\n",
		    progname, ERR_FILE_NOT_FOUND);
		return (3);
	}

	if (fi_read(path, nofollow, info) != 0) {
		(void)fprintf(stdout, "%s: could not refer to file (%d)\n",
		    progname, ERR_FILE_NOT_FOUND);
		return (3);
	}

	if (realpath(path, resolved) == NULL)
		(void)strncpy(resolved, path, sizeof(resolved) - 1);

	switch (field) {
	case 't':
		fi_format_code(info, code, sizeof(code));
		(void)printf("\"%s\"\n", code);
		return (0);
	case 'c':
		fi_format_code(info + 4, code, sizeof(code));
		(void)printf("\"%s\"\n", code);
		return (0);
	case 'a':
		if (attr != NULL) {
			(void)printf("%d\n",
			    fi_attr_get(info, st.st_flags, attr) ? 1 : 0);
			return (0);
		}
		print_attributes(info, st.st_flags);
		return (0);
	case 'd':
		format_time(st.st_birthtime, stamp, sizeof(stamp));
		(void)printf("%s\n", stamp);
		return (0);
	case 'm':
		format_time(st.st_mtime, stamp, sizeof(stamp));
		(void)printf("%s\n", stamp);
		return (0);
	}

	/* Every field. */
	(void)printf("%s: \"%s\"\n",
	    S_ISDIR(st.st_mode) ? "directory" : "file", resolved);

	if (!S_ISDIR(st.st_mode)) {
		fi_format_code(info, code, sizeof(code));
		(void)printf("type: \"%s\"\n", code);
		fi_format_code(info + 4, code, sizeof(code));
		(void)printf("creator: \"%s\"\n", code);
	}

	(void)printf("attributes: ");
	print_attributes(info, st.st_flags);

	format_time(st.st_birthtime, stamp, sizeof(stamp));
	(void)printf("created: %s\n", stamp);
	format_time(st.st_mtime, stamp, sizeof(stamp));
	(void)printf("modified: %s\n", stamp);

	return (0);
}
