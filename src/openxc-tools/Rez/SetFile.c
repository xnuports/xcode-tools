/*
 * SetFile -- set a file's Finder metadata.
 *
 *	SetFile [-a attributes] [-c creator] [-d date] [-m date] [-P]
 *		[-t type] file...
 *
 * Attributes are given as a string of letters, uppercase to set and
 * lowercase to clear.  Dates are mm/dd/[yy]yy with an optional time.
 *
 * "l" is the odd one: locking a file is chflags(2), not a Finder bit, so it
 * is applied separately -- and last, since setting it first would stop the
 * other changes being written.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "finderinfo.h"

static const char *progname = "SetFile";

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [option...] file...\n", progname);
	(void)fprintf(stderr,
	    "    -a attributes     # attributes (lowercase = 0, uppercase = 1)*\n"
	    "    -c creator        # file creator\n"
	    "    -d date           # creation date (mm/dd/[yy]yy [hh:mm[:ss] [AM | PM]])*\n"
	    "    -m date           # modification date (mm/dd/[yy]yy [hh:mm[:ss] [AM | PM]])*\n"
	    "    -P                # perform action on symlink instead of following it\n"
	    "    -t type           # file type\n");
}

/*
 * mm/dd/[yy]yy with an optional time and an optional AM/PM.  A missing time
 * is midnight and a two-digit year is 20xx.
 *
 * This is one of the few places that deliberately does not match Apple's,
 * because Apple's is wrong.  Theirs, measured:
 *
 *	-d 07/04/2018	->  07/04/2018 07:04:00
 *	-d 11/23/1999	->  11/23/1999 11:23:00
 *	-d 12/31/07	->  12/31/2007 12:31:07
 *	-d 05/06/45	->  03/30/1909 22:38:29
 *
 * With no time given the month and day come back as the hour and minute,
 * and a two-digit year below some threshold takes the date with it.  It is
 * repeatable -- three runs of each give the same answer -- but it is not
 * behaviour worth reproducing: a date-only argument is documented and
 * ordinary, and every one of those answers is wrong.
 */
static bool
parse_date(const char *s, time_t *out)
{
	struct tm tm;
	int month, day, year, hour = 0, minute = 0, second = 0;
	char meridian[3] = "";
	int n;

	n = sscanf(s, "%d/%d/%d %d:%d:%d %2s",
	    &month, &day, &year, &hour, &minute, &second, meridian);
	if (n < 3)
		return (false);
	if (n == 5 || n == 4)
		second = 0;

	if (year < 100)
		year += 2000;

	if (meridian[0] == 'P' || meridian[0] == 'p') {
		if (hour < 12)
			hour += 12;
	} else if (meridian[0] == 'A' || meridian[0] == 'a') {
		if (hour == 12)
			hour = 0;
	}

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_isdst = -1;

	*out = mktime(&tm);
	return (*out != (time_t)-1);
}

/*
 * A type or creator is exactly four characters.  Anything else is reported
 * and then ignored -- the option is dropped, the rest of the command still
 * runs, and the exit status stays zero, which is what Apple's does.  An
 * empty one is dropped without a word.
 */
static bool
valid_code(const char *value)
{
	if (value[0] == '\0')
		return (false);

	if (strlen(value) != 4) {
		(void)printf("Invalid type or creator value: '%s'\n", value);
		return (false);
	}
	return (true);
}

static void
copy_code(uint8_t *dst, const char *src)
{
	memcpy(dst, src, 4);
}

static int
set_creation_time(const char *path, bool nofollow, time_t when)
{
	struct attrlist al;
	struct timespec ts;

	memset(&al, 0, sizeof(al));
	al.bitmapcount = ATTR_BIT_MAP_COUNT;
	al.commonattr = ATTR_CMN_CRTIME;

	ts.tv_sec = when;
	ts.tv_nsec = 0;

	return (setattrlist(path, &al, &ts, sizeof(ts),
	    nofollow ? FSOPT_NOFOLLOW : 0));
}

static int
set_modification_time(const char *path, time_t when)
{
	struct timeval tv[2];
	struct stat st;

	if (stat(path, &st) != 0)
		return (-1);

	tv[0].tv_sec = st.st_atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = when;
	tv[1].tv_usec = 0;

	return (utimes(path, tv));
}

int
main(int argc, char *argv[])
{
	const char *attrs = NULL, *creator = NULL, *type = NULL;
	time_t created = 0, modified = 0;
	bool set_created = false, set_modified = false;
	bool nofollow = false;
	int status = 0;
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || argv[i][1] == '\0')
			break;

		switch (argv[i][1]) {
		case 'P':
			nofollow = true;
			continue;
		case 'a':
		case 'c':
		case 't':
		case 'd':
		case 'm':
			break;
		default:
			(void)fprintf(stderr, "Invalid Argument %s (null)\n",
			    argv[i]);
			return (1);
		}

		if (i + 1 >= argc) {
			usage();
			return (1);
		}

		switch (argv[i][1]) {
		case 'a':
			attrs = argv[++i];
			break;
		case 'c':
			creator = argv[++i];
			if (!valid_code(creator))
				creator = NULL;
			break;
		case 't':
			type = argv[++i];
			if (!valid_code(type))
				type = NULL;
			break;
		case 'd':
			if (!parse_date(argv[++i], &created)) {
				(void)fprintf(stderr,
				    "%s: invalid date %s\n", progname, argv[i]);
				return (1);
			}
			set_created = true;
			break;
		case 'm':
			if (!parse_date(argv[++i], &modified)) {
				(void)fprintf(stderr,
				    "%s: invalid date %s\n", progname, argv[i]);
				return (1);
			}
			set_modified = true;
			break;
		}
	}

	if (i >= argc) {
		usage();
		return (1);
	}

	for (; i < argc; i++) {
		const char *path = argv[i];
		uint8_t info[FINDERINFO_SIZE];
		struct stat st;
		bool touch_info = (attrs != NULL || creator != NULL ||
		    type != NULL);
		bool lock = false, unlock = false;

		if ((nofollow ? lstat(path, &st) : stat(path, &st)) != 0) {
			(void)fprintf(stderr,
			    "ERROR: Unexpected Error. (-5000)  on file: %s \n",
			    path);
			status = 1;
			continue;
		}

		/*
		 * An already-locked file has to be unlocked before anything
		 * can be written to it, including its own attributes.
		 */
		if ((st.st_flags & UF_IMMUTABLE) != 0)
			(void)chflags(path, st.st_flags & ~(unsigned long)UF_IMMUTABLE);

		if (touch_info && fi_read(path, nofollow, info) != 0) {
			(void)fprintf(stderr,
			    "ERROR: Unexpected Error. (-5000)  on file: %s \n",
			    path);
			status = 1;
			continue;
		}

		if (type != NULL)
			copy_code(info, type);
		if (creator != NULL)
			copy_code(info + 4, creator);

		if (attrs != NULL) {
			const char *p;

			for (p = attrs; *p != '\0'; p++) {
				const struct fi_attr *a = fi_attr_find(*p);
				bool on = (*p >= 'A' && *p <= 'Z');

				if (a == NULL) {
					(void)fprintf(stderr,
					    "Invalid Argument %c (null)\n", *p);
					status = 1;
					continue;
				}
				if (a->offset == FI_ATTR_LOCKED) {
					lock = on;
					unlock = !on;
					continue;
				}
				fi_attr_set(info, a, on);
			}
		}

		if (touch_info && fi_write(path, nofollow, info) != 0) {
			(void)fprintf(stderr,
			    "ERROR: Unexpected Error. (-5000)  on file: %s \n",
			    path);
			status = 1;
		}

		if (set_created && set_creation_time(path, nofollow, created) != 0)
			status = 1;
		if (set_modified && set_modification_time(path, modified) != 0)
			status = 1;

		/* Locking last, so the writes above still had somewhere to go. */
		if (lock)
			(void)chflags(path, st.st_flags | UF_IMMUTABLE);
		else if (unlock)
			(void)chflags(path, st.st_flags & ~(unsigned long)UF_IMMUTABLE);
	}

	return (status);
}
