/*
 * ResMerger -- combine resource files into one.
 *
 *	ResMerger [-fileCreator <c>] [-fileType <t>] [-a[ppend]]
 *		  [-skip <type>]... [-srcIs RSRC | DF] [-dstIs RSRC | DF]
 *		  <source>... -o <dest>
 *
 * Sources are read in order and the first of any duplicate wins, with a
 * warning naming the type and id.  Types come out in the order they were
 * first seen.
 *
 * Copyright (c) 2026 Sunneva N. Mariu <sunnevanattsol@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/stat.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "finderinfo.h"
#include "resfork.h"

#define MAX_RESOURCES	4096
#define MAX_SOURCES	256
#define MAX_SKIPS	64

static const char *progname;	/* argv[0]: Apple's warnings quote it whole */

static void
usage(void)
{
	(void)printf("usage: ResMerger [-fileCreator <fileCreator>] "
	    "[-fileType <fileType>] [-{a}ppend] [-skip <type>]... "
	    "[-srcIs RSRC | DF] [-dstIs RSRC | DF] <source-file1> "
	    "[<source-file2>...] -o <dest-file>\n");
}

/* Four characters, space-padded, as a resource type is spelled. */
static void
type_of(const char *s, char out[5])
{
	size_t i, len = strlen(s);

	for (i = 0; i < 4; i++)
		out[i] = (char)(i < len ? s[i] : ' ');
	out[4] = '\0';
}

int
main(int argc, char *argv[])
{
	struct resource merged[MAX_RESOURCES];
	const char *sources[MAX_SOURCES];
	char skips[MAX_SKIPS][5];
	uint8_t *buffers[MAX_SOURCES];
	struct resfork forks[MAX_SOURCES];
	const char *dest = NULL, *filetype = NULL, *filecreator = NULL;
	bool src_df = false, dst_df = false, append = false;
	size_t nsources = 0, nskips = 0, nmerged = 0, i, j, k;
	uint16_t fileref = RESFORK_FILEREF_NEW;
	uint8_t *out;
	size_t outlen;
	int status = 0;

	progname = argv[0];

	for (i = 1; i < (size_t)argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "-o") == 0 && i + 1 < (size_t)argc) {
			dest = argv[++i];
		} else if (strcmp(a, "-skip") == 0 && i + 1 < (size_t)argc) {
			if (nskips < MAX_SKIPS)
				type_of(argv[++i], skips[nskips++]);
			else
				i++;
		} else if (strcmp(a, "-fileType") == 0 && i + 1 < (size_t)argc) {
			filetype = argv[++i];
		} else if (strcmp(a, "-fileCreator") == 0 &&
		    i + 1 < (size_t)argc) {
			filecreator = argv[++i];
		} else if (strcmp(a, "-srcIs") == 0 && i + 1 < (size_t)argc) {
			src_df = (strcmp(argv[++i], "DF") == 0);
		} else if (strcmp(a, "-dstIs") == 0 && i + 1 < (size_t)argc) {
			dst_df = (strcmp(argv[++i], "DF") == 0);
		} else if (strcmp(a, "-a") == 0 || strcmp(a, "-append") == 0) {
			append = true;
		} else if (a[0] == '-') {
			(void)printf("### %s - ERROR: unknown argument: %s\n",
			    progname, a);
			usage();
			return (0);
		} else if (nsources < MAX_SOURCES) {
			sources[nsources++] = a;
		}
	}

	/*
	 * A destination and no sources is not an error: it writes an empty
	 * resource file, which is what Apple's does.
	 */
	if (dest == NULL) {
		usage();
		return (0);
	}

	/*
	 * Appending starts from what the destination already holds, so its
	 * resources are the first read and therefore win any duplicate.  It
	 * also changes the reference number written into the map, because
	 * the destination had to be opened before it was rewritten.
	 */
	if (append && access(dest, F_OK) == 0)
		fileref = RESFORK_FILEREF_REOPEN;

	if (append && nsources < MAX_SOURCES) {
		for (i = nsources; i > 0; i--)
			sources[i] = sources[i - 1];
		sources[0] = dest;
		nsources++;
	}

	for (i = 0; i < nsources; i++) {
		const char *why = "";
		size_t len;

		buffers[i] = resfork_read_file(sources[i], src_df, &len);
		if (buffers[i] == NULL ||
		    resfork_parse(buffers[i], len, &forks[i], &why) != 0) {
			(void)printf("file %s: ### %s - Fatal Error: could not "
			    "be read\n", sources[i], progname);
			free(buffers[i]);
			buffers[i] = NULL;
			forks[i].items = NULL;
			forks[i].count = 0;
			status = 1;
			continue;
		}

		for (j = 0; j < forks[i].count; j++) {
			struct resource *r = &forks[i].items[j];
			bool skip = false;

			for (k = 0; k < nskips; k++)
				if (memcmp(skips[k], r->type, 4) == 0)
					skip = true;
			if (skip)
				continue;

			for (k = 0; k < nmerged; k++)
				if (merged[k].id == r->id &&
				    memcmp(merged[k].type, r->type, 4) == 0) {
					(void)printf("file %s: ### %s - "
					    "Warning: Duplicate resource "
					    "(resType '%s' ID %d)\n",
					    sources[i], progname, r->type,
					    r->id);
					skip = true;
					break;
				}
			if (skip || nmerged >= MAX_RESOURCES)
				continue;

			merged[nmerged++] = *r;
		}
	}

	out = resfork_build(merged, nmerged, fileref, &outlen);
	if (out == NULL) {
		(void)printf("### %s - Fatal Error: out of memory\n", progname);
		return (1);
	}

	if (resfork_write_file(dest, dst_df, out, outlen) != 0) {
		(void)printf("### %s - Fatal Error: could not write \"%s\"\n",
		    progname, dest);
		status = 1;
	}
	free(out);

	if (filetype != NULL || filecreator != NULL) {
		uint8_t info[FINDERINFO_SIZE];

		if (fi_read(dest, false, info) == 0) {
			char code[5];

			if (filetype != NULL) {
				type_of(filetype, code);
				memcpy(info, code, 4);
			}
			if (filecreator != NULL) {
				type_of(filecreator, code);
				memcpy(info + 4, code, 4);
			}
			(void)fi_write(dest, false, info);
		}
	}

	for (i = 0; i < nsources; i++) {
		resfork_free(&forks[i]);
		free(buffers[i]);
	}

	return (status);
}
